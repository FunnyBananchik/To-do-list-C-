#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include <cctype>
#include <codecvt>
#include <algorithm>
#include "Todo.h"
#include "Categories.h"
#include "AuthService.h"
#include "User.h"
#include "Logger.h"

using namespace std;

#ifdef BUILD_TESTS
    #define DATABASE_INSTANCE extern
#else
    #define DATABASE_INSTANCE
#endif

class Database {
    private:
        sqlite3* db;
        string db_path;
        static Database* instance;
        Database() : db(nullptr) {}
    public:
        static Database* getInstance() {
            if (!instance) {
             instance = new Database();
            }
            return instance;
        } 

        static void cleanup() {
            if (instance) {
                delete instance;
                instance = nullptr;
            }
        }
        
        bool open(const string& path) {
            db_path = path;

            const char* sql_users = R"(
                CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                salt TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                );
            )";

            const char* sql_todos = R"(
                CREATE TABLE IF NOT EXISTS todos (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                description TEXT,
                completed BOOLEAN DEFAULT 0,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                due_date DATETIME DEFAULT NULL,
                priority INTEGER DEFAULT 1,
                category_id INTEGER DEFAULT 0,
                user_id INTEGER NOT NULL,
                FOREIGN KEY (category_id) REFERENCES categories(id) ON DELETE SET NULL,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
                );
            )";
            
            const char* sql_cat = R"(CREATE TABLE IF NOT EXISTS categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                color TEXT NOT NULL,
                user_id INTEGER NOT NULL,
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
                UNIQUE(user_id, name)
                );
            )";

             // Создание виртуальной таблицы FTS5 для поиска
            const char* createFtsTable = R"(
            CREATE VIRTUAL TABLE IF NOT EXISTS todos_fts USING fts5(
                id UNINDEXED,
                user_id UNINDEXED,
                title,
                description,
                content='todos',
                content_rowid='rowid',
                tokenize = 'unicode61 remove_diacritics 1 tokenchars ''абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ''',
                prefix='2,3'
                );
            )";
        
            createTables(createFtsTable);

            int rc = sqlite3_open(path.c_str(), &db);
            if (rc != SQLITE_OK) {
                app_logger.error("Cannot open database: " + string(sqlite3_errstr(rc)));
                return false;
            }
            stringstream ss;
            ss << "Database opened: " << path;
            app_logger.info(ss.str());
            createTables(sql_users);
            createTables(sql_cat);
            createTables(sql_todos);

        // Триггеры для автоматического обновления FTS
            const char* sql_fts_triggers = R"(
                        -- Триггер для INSERT
            CREATE TRIGGER IF NOT EXISTS todos_ai AFTER INSERT ON todos BEGIN
                INSERT INTO todos_fts(rowid, id, user_id, title, description) 
                VALUES (new.rowid, new.id, new.user_id, new.title, new.description);
            END;
            
            -- Триггер для UPDATE
            CREATE TRIGGER IF NOT EXISTS todos_au AFTER UPDATE ON todos BEGIN
                DELETE FROM todos_fts WHERE rowid = old.rowid;
                INSERT INTO todos_fts(rowid, id, user_id, title, description) 
                VALUES (new.rowid, new.id, new.user_id, new.title, new.description);
            END;
            
            -- Триггер для DELETE
            CREATE TRIGGER IF NOT EXISTS todos_ad AFTER DELETE ON todos BEGIN
                DELETE FROM todos_fts WHERE rowid = old.rowid;
            END;
            )";
            createTables(sql_fts_triggers);
            checkAndPopulateFtsTable();
            if (countUsers() == 0) {
                createDefaultUser();
            }

                return true;
        }

        void close() {
            if (db) {
                 sqlite3_close(db);
                db = nullptr;
            }
        }

        sqlite3* getRawDb() const {
        return db;
    }

        void createTables(const char* sql) {
            char* errMsg = nullptr;
            int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                app_logger.error("SQL error: " + string(sqlite3_errstr(rc)));
                sqlite3_free(errMsg);
            } else {
                app_logger.debug("Tables created/checked");
            }
        }

        void checkAndPopulateFtsTable() {
        // Проверяем, существует ли таблица FTS5
        const char* check_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='todos_fts'";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr) != SQLITE_OK) {
            app_logger.error("Failed to check FTS table: ");
            return;
        }
        
        bool fts_exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fts_exists = true;
        }
        sqlite3_finalize(stmt);
        
        if (!fts_exists) {
            app_logger.debug("FTS table not found, creating...");
            const char* create_fts = R"(
                CREATE VIRTUAL TABLE todos_fts USING fts5(
                    id UNINDEXED,
                    user_id UNINDEXED,
                    title,
                    description,
                    content='todos',
                    content_rowid='rowid'

                )
            )";
            
            char* errMsg = nullptr;
            if (sqlite3_exec(db, create_fts, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                app_logger.error("Failed to create FTS table: ");
                sqlite3_free(errMsg);
                return;
            }
            app_logger.debug("FTS table created");
        }
            populateFtsTable();
        }


        void populateFtsTable() {
        
            const char* clear_sql = "DELETE FROM todos_fts";
            sqlite3_exec(db, clear_sql, nullptr, nullptr, nullptr);
            
            const char* sql = R"(
                INSERT INTO todos_fts(rowid, id, user_id, title, description)
                SELECT rowid, id, user_id, title, description FROM todos
                WHERE rowid NOT IN (SELECT rowid FROM todos_fts)
            )";
            
            char* errMsg = nullptr;
            int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                app_logger.error("Error populating FTS table: " + string(sqlite3_errstr(rc)));
                sqlite3_free(errMsg);
            } else {
                app_logger.debug("FTS table populated with existing data");
            }
        }

        //_______________________________Поиск_________________________________________

    vector<Todo> searchTodos(int user_id, const string& query, bool highlight = false) {
        vector<Todo> results;
    
        if (query.empty()) {
            return results;
        }
        
        string fts_query = escapeFtsQuery(query);
        
        string sql = R"(
            SELECT 
                t.id,
                t.title,
                t.description,
                t.completed,
                t.created_at,
                t.due_date,
                t.priority,
                t.category_id,
                t.user_id,
                bm25(todos_fts) as relevance
            FROM todos t
            JOIN todos_fts ON t.id = todos_fts.rowid
            WHERE t.user_id = ?
            AND todos_fts MATCH ?
            ORDER BY relevance DESC, t.created_at DESC
            LIMIT 50
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return results;
        }
        
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, fts_query.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo todo;
            int col = 0;
            
            todo.id = sqlite3_column_int(stmt, col++);
            
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col++));
            todo.title = title ? title : "";
            
            const char* description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col++));
            todo.description = description ? description : "";
            
            todo.completed = sqlite3_column_int(stmt, col++) == 1;
            
            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col++));
            todo.created_at = created_at ? created_at : "";
            
            const char* due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col++));
            todo.due_date = due_date ? due_date : "";
            
            todo.priority = sqlite3_column_int(stmt, col++);
            todo.category_id = sqlite3_column_int(stmt, col++);
            todo.user_id = sqlite3_column_int(stmt, col++);
            
            todo.relevance = sqlite3_column_double(stmt, col++);
            
            results.push_back(todo);
        }
        
        sqlite3_finalize(stmt);
        
        if (highlight && !results.empty()) {
            vector<string> search_terms = extractSearchTermsSimple(query);
            
            for (auto& todo : results) {
                todo.title_snippet = createHighlightedSnippet(todo.title, search_terms);
                if (!todo.description.empty()) {
                    todo.desc_snippet = createHighlightedSnippet(todo.description, search_terms);
                }
            }
        }
        
        if (results.empty()) {
            results = searchTodosFallback(user_id, query);
            
            if (highlight && !results.empty()) {
                vector<string> search_terms = extractSearchTermsSimple(query);
                for (auto& todo : results) {
                    todo.title_snippet = createHighlightedSnippet(todo.title, search_terms);
                    if (!todo.description.empty()) {
                        todo.desc_snippet = createHighlightedSnippet(todo.description, search_terms);
                    }
                }
            }
        }
        
        return results;
    }

string createHighlightedSnippet(const string& text, const vector<string>& search_terms) {
    if (text.empty() || search_terms.empty()) {
        return text;
    }
    string result = text;
    
    for (const auto& term : search_terms) {
        if (term.length() < 2) continue;
        
        string result_lower = toLowerSimple(result);
        string term_lower = toLowerSimple(term);
        
        size_t pos = 0;
        int offset = 0;
        
        while ((pos = result_lower.find(term_lower, pos)) != string::npos) {
            result.insert(pos + offset, "<mark>");
            offset += 6; 
            
            result.insert(pos + offset + term.length(), "</mark>");
            offset += 7;
            pos += term.length() + 13; 
            
            result_lower = toLowerSimple(result);
        }
    }
    
    return result;
}

// Конвертер UTF-8 <-> UTF-32
std::wstring utf8_to_wstring(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

std::string toLowerSimple(const std::string& str) {
    std::wstring wstr = utf8_to_wstring(str);
    
    for (wchar_t& c : wstr) {
        if (c >= L'A' && c <= L'Z') {
            c = c + (L'a' - L'A');
        }
        else if (c >= L'А' && c <= L'Я') {
            c = c + (L'а' - L'А');
        }
        else if (c == L'Ё') {
            c = L'ё';
        }
    }
    
    return wstring_to_utf8(wstr);
}

vector<string> extractSearchTermsSimple(const string& query) {
    vector<string> terms;
    
    string clean_query = query;
    
    if (clean_query.length() >= 2 && clean_query.front() == '"' && clean_query.back() == '"') {
        clean_query = clean_query.substr(1, clean_query.length() - 2);
    }
    istringstream iss(clean_query);
    string word;
    while (iss >> word) {
        if (word.length() < 2) continue;
        
        if (!word.empty() && word.back() == '*') {
            word.pop_back();
        }
        
        while (!word.empty() && ispunct(word.back())) {
            word.pop_back();
        }
        
        if (!word.empty()) {
            terms.push_back(word);
        }
    }
    
    return terms;
}

    vector<Todo> searchTodosFallback(int user_id, const string& query) {
        vector<Todo> results;
        
        if (query.empty()) {
            return results;
        }
        
        string sql = "SELECT id, title, description, completed, created_at, due_date, priority, category_id, user_id FROM todos WHERE user_id = ? AND (title LIKE ? OR description LIKE ?) ORDER BY created_at DESC LIMIT 50";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return results;
        }
        
        string pattern = "%" + query + "%";
        
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo todo;
            todo.id = sqlite3_column_int(stmt, 0);
            
            const char* title = (const char*)sqlite3_column_text(stmt, 1);
            todo.title = title ? title : "";
            
            const char* description = (const char*)sqlite3_column_text(stmt, 2);
            todo.description = description ? description : "";
            
            todo.completed = sqlite3_column_int(stmt, 3) == 1;
            
            const char* created_at = (const char*)sqlite3_column_text(stmt, 4);
            todo.created_at = created_at ? created_at : "";
            
            const char* due_date = (const char*)sqlite3_column_text(stmt, 5);
            todo.due_date = due_date ? due_date : "";
            
            todo.priority = sqlite3_column_int(stmt, 6);
            todo.category_id = sqlite3_column_int(stmt, 7);
            todo.user_id = sqlite3_column_int(stmt, 8);
            todo.relevance = 1.0;
            
            results.push_back(todo);
        }
        
        sqlite3_finalize(stmt);
        return results;
    }

    string escapeFtsQuery(const string& query) {
        if (query.empty()) return "";
        
        string escaped;
        escaped.reserve(query.length());
        
        const string special_chars = "\"'\\^~-:";
        
        for (char c : query) {
            if (c != '*' && special_chars.find(c) != string::npos) {
                escaped += '\\';
            }
            escaped += c;
        }
        
        bool has_operators = 
        escaped.find(" AND ") != string::npos ||
        escaped.find(" OR ") != string::npos ||
        escaped.find(" NOT ") != string::npos;
    
        bool is_phrase = escaped.length() >= 2 && 
                        escaped.front() == '"' && 
                        escaped.back() == '"';

        bool has_prefix = escaped.find('*') != string::npos;

        if (has_prefix) {
            if (escaped.back() != '*') {
                size_t star_pos = escaped.find('*');
                if (star_pos != string::npos) {
                    if (star_pos < escaped.length() - 1 && escaped[star_pos + 1] == ' ') {
                        escaped = escaped.substr(0, star_pos + 1);
                    }
                }
            }
        }
        return escaped;
    }

        //_______________________________Todo__________________________________________

        Todo createTodo(const string& title, const string& description = "", const int& priority = 1, const string& due_date = "", const int& category_id = 0, int user_id =0) {
            const char* sql = "INSERT INTO todos (title, description, due_date, priority, category_id, user_id) VALUES (?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;
            
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return Todo("", "");
            }
            
            sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, due_date.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, priority);
            sqlite3_bind_int(stmt, 5, category_id);
            sqlite3_bind_int(stmt, 6, user_id);
            
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                stringstream ss;
                ss << "Failed to insert: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                sqlite3_finalize(stmt);
                return Todo("", "");
            }
            
            int newId = sqlite3_last_insert_rowid(db);
                    sqlite3_finalize(stmt);
        
            // Получить созданную задачу из БД
            return getTodoById(newId, user_id);
        }

        vector<Todo> getUserTodos(int user_id) {
            vector<Todo> Todos;
            const char* sql = "Select id, title, description, completed, created_at, due_date, priority, category_id From todos WHERE user_id = ? ORDER BY created_at DESC";
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return Todos;
            }
            sqlite3_bind_int(stmt, 1, user_id);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Todo todo;
                todo.id = sqlite3_column_int(stmt, 0);
                todo.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                todo.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                todo.completed = sqlite3_column_int(stmt, 3) == 1;
                const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    if (created_at) {
                        todo.created_at = created_at;
                    } else {
                        todo.created_at = "Unknown";
                    }
                todo.due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                todo.priority = sqlite3_column_int(stmt, 6);
                todo.category_id = sqlite3_column_int(stmt, 7);
                Todos.push_back(todo);
            }
            sqlite3_finalize(stmt);
            return Todos;
        }

        Todo getTodoById(int id, int user_id) {
            const char* sql = "SELECT id, title, description, completed, created_at, due_date, priority, category_id FROM todos WHERE id = ? and user_id = ?;";
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return Todo("", "");
            }
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_bind_int(stmt, 2, user_id);
            Todo todo("", "");

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                todo.id = sqlite3_column_int(stmt, 0);
                todo.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                todo.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                todo.completed = sqlite3_column_int(stmt, 3) == 1;
            
                const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                if (created_at) {
                    todo.created_at = created_at;
                }
                todo.due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                todo.priority = sqlite3_column_int(stmt, 6);
                todo.category_id = sqlite3_column_int(stmt, 7);
            }
        
        sqlite3_finalize(stmt);
        return todo;
    }


    bool updateTodo (const Todo& todo) {
        const char* sql = "UPDATE todos SET title = ?, description = ?, completed = ?, due_date = ?, priority = ?, category_id = ? WHERE id = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return false;
        }
        sqlite3_bind_text(stmt, 1, todo.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, todo.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, todo.completed ? 1 : 0);
        sqlite3_bind_text(stmt, 4, todo.due_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, todo.priority);
        sqlite3_bind_int(stmt, 6, todo.category_id);
        sqlite3_bind_int(stmt, 7, todo.id);

        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);

        if (!success) {
            stringstream ss;
            ss << "Failed to update: "<< sqlite3_errmsg(db);
            app_logger.error(ss.str());
        }
        
        sqlite3_finalize(stmt);
        return success;
    }

      bool deleteTodo(int id) {
        const char* sql = "DELETE FROM todos WHERE id = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (!success) {
            stringstream ss;
            ss << "Failed to delete: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
        }
        
        sqlite3_finalize(stmt);

        if (success) {
            int changes = sqlite3_changes(db);
            return changes > 0; 
        }
        return success;
    }

    vector<Todo> getOverdueTrue (int user_id){
        vector<Todo> todos_temp;
        const char* sql = "SELECT id, title, description, completed, created_at, due_date, priority, category_id FROM todos WHERE due_date < date('now') AND completed = 0 AND due_date != '' AND user_id = ? ORDER BY due_date ASC;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return todos_temp;
        }
        sqlite3_bind_int(stmt, 1, user_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
                Todo todo;
                todo.id = sqlite3_column_int(stmt, 0);
                todo.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                todo.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                todo.completed = sqlite3_column_int(stmt, 3) == 1;
                const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    if (created_at) {
                        todo.created_at = created_at;
                    } else {
                        todo.created_at = "Unknown";
                    }
                todo.due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                todo.priority = sqlite3_column_int(stmt, 6);
                todo.category_id = sqlite3_column_int(stmt, 7);
                todos_temp.push_back(todo);
            }
            sqlite3_finalize(stmt);
            return todos_temp;
    }


    vector<Todo> getExpirationTodo (int user_id){
        vector<Todo> todos_temp;
        const char* sql = "SELECT id, title, description, completed, created_at, due_date, priority, category_id FROM todos WHERE due_date >= datetime('now') AND due_date <= datetime('now', '+3 days') AND completed = 0 AND user_id = ? ORDER BY due_date ASC;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return todos_temp;
        }
        sqlite3_bind_int(stmt, 1, user_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
                Todo todo;
                todo.id = sqlite3_column_int(stmt, 0);
                todo.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                todo.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                todo.completed = sqlite3_column_int(stmt, 3) == 1;
                const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    if (created_at) {
                        todo.created_at = created_at;
                    } else {
                        todo.created_at = "Unknown";
                    }
                todo.due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                todo.priority = sqlite3_column_int(stmt, 6);
                todo.category_id = sqlite3_column_int(stmt, 7);
                todos_temp.push_back(todo);
            }
            sqlite3_finalize(stmt);
            return todos_temp;
    }

    struct Stats {
        int total;
        int completed;
        int pending;
    };

     Stats getStats(int user_id) {
        Stats stats = {0, 0, 0};
        const char* sql_total = "SELECT COUNT(*) FROM todos WHERE user_id = ?;";
        const char* sql_completed = "SELECT COUNT(*) FROM todos WHERE completed = 1 AND user_id = ?;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql_total, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        sqlite3_prepare_v2(db, sql_completed, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.completed = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        
        stats.pending = stats.total - stats.completed;
        
        return stats;

    }

    bool replaceTodo(const Todo& todo, int user_id) {       
        const char* sql = R"(
            INSERT OR REPLACE INTO todos (id, title, description, completed, created_at, due_date, priority, category_id, user_id)
            VALUES (?, ?, ?, ?, COALESCE((SELECT created_at FROM todos WHERE id = ? AND user_id = ?), CURRENT_TIMESTAMP), ?, ?, ?, ?);
        )";
        
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, todo.id);
        sqlite3_bind_text(stmt, 2, todo.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, todo.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, todo.completed ? 1 : 0);
        sqlite3_bind_int(stmt, 5, todo.id);
        sqlite3_bind_int(stmt, 6, todo.user_id);
        sqlite3_bind_text(stmt, 7, todo.due_date.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 8, todo.priority);
        sqlite3_bind_int(stmt, 9, todo.category_id);
        sqlite3_bind_int(stmt, 10, todo.user_id);

        
        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (success) {
            app_logger.info("Todo replaced, ID: " + to_string(todo.id));
        } else {
            app_logger.error("Failed to replace todo ID " + to_string(todo.id) + ": " + string(sqlite3_errstr(rc)));
        }
        
        sqlite3_finalize(stmt);
        return success;
    }

    vector<Todo> getTodosByCategories(const vector<int>& category_ids, int user_id) {
        vector<Todo> todos;
        
        if (category_ids.empty()) {
            return todos;
        }
        
        stringstream sql;
        sql << "SELECT id, title, description, completed, created_at, due_date, priority, category_id FROM todos WHERE category_id IN (";
        
        for (size_t i = 0; i < category_ids.size(); ++i) {
            sql << "?";
            if (i != category_ids.size() - 1) {
                sql << ",";
            }
        }
        sql << ") AND user_id = ? ORDER BY created_at DESC";
        
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return todos;
        }
        
        for (size_t i = 0; i < category_ids.size(); ++i) {
            sqlite3_bind_int(stmt, i + 1, category_ids[i]);
        }
        sqlite3_bind_int(stmt, category_ids.size() + 1, user_id);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo todo;
            todo.id = sqlite3_column_int(stmt, 0);
            todo.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            todo.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            todo.completed = sqlite3_column_int(stmt, 3) == 1;
            
            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            if (created_at) {
                todo.created_at = created_at;
            } else {
                todo.created_at = "Unknown";
            }
            
            todo.due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            todo.priority = sqlite3_column_int(stmt, 6);
            todo.category_id = sqlite3_column_int(stmt, 7);
            
            todos.push_back(todo);
        }
        
        sqlite3_finalize(stmt);
    return todos;
}


    /*________________________________Функции для таблицы категорий___________________________*/

    categories createCat(const string& name, const string& color, int user_id) {
            const char* sql = "INSERT INTO categories (name, color, user_id) VALUES (?, ?, ?);";
            sqlite3_stmt* stmt;
            
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return categories("", "", 0);
            }
            
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, color.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, user_id);
            
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                stringstream ss;
                ss << "Failed to insert: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                sqlite3_finalize(stmt);
                return categories("", "", 0);
            }
            
            int newId = sqlite3_last_insert_rowid(db);
                    sqlite3_finalize(stmt);
        
            // Получить созданную категорию из БД
            return getCatById(newId);
        }

        vector<categories> getAllCat(int user_id) {
            vector<categories> cat;
            const char* sql = "Select id, name, color FROM categories WHERE user_id = ? ORDER BY id DESC";
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return cat;
            }
            sqlite3_bind_int(stmt, 1, user_id);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                categories cat_exam;
                cat_exam.id = sqlite3_column_int(stmt, 0);
                cat_exam.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                cat_exam.color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                cat.push_back(cat_exam);
            }
            sqlite3_finalize(stmt);
            return cat;
        }

        categories getCatById(int id) {
            const char* sql = "SELECT id, name, color FROM categories WHERE id = ?;";
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                stringstream ss;
                ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
                app_logger.error(ss.str());
                return categories("", "", 0);
            }
            sqlite3_bind_int(stmt, 1, id);
            categories cat_exam("", "", 0);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                cat_exam.id = sqlite3_column_int(stmt, 0);
                cat_exam.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                cat_exam.color = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            }
        
        sqlite3_finalize(stmt);
        return cat_exam;
    }


    bool deleteCat(int id) {
        const char* sql = "DELETE FROM categories WHERE id = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (!success) {
            stringstream ss;
            ss << "Failed to delete: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
        }
        
        sqlite3_finalize(stmt);
        return success;
    }

    // _____________________________ДЛЯ ПОЛЬЗОВАТЕЛЕЙ____________________________________

    User registerUser(const string& username, const string& password) {
        // Генерируем соль и хэш пароля
        string salt = AuthService::generateSalt();
        string password_hash = AuthService::hashPassword(password, salt);
        
        const char* sql = "INSERT INTO users (username, password_hash, salt) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return User();
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            stringstream ss;
            ss << "Failed to register user: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            sqlite3_finalize(stmt);
            return User();
        }
        
        int newId = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        app_logger.info("User registered: " + username + " (ID: " + to_string(newId) + ")");
        return getUserById(newId);
    }

    User loginUser(const string& username, const string& password) {
        const char* sql = "SELECT id, username, password_hash, salt FROM users WHERE username = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return User();
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        
        User user;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id = sqlite3_column_int(stmt, 0);
            user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            string input_hash = AuthService::hashPassword(password, salt);
            
            if (input_hash == stored_hash) {
                user.password_hash = stored_hash;
                user.salt = salt;
                app_logger.info("User authenticated: " + username);
            } else {
                app_logger.warn("Invalid password for user: " + username);
                user.id = 0; // Сбрасываем ID при неудачной аутентификации
            }
        } else {
            app_logger.warn("User not found: ");
        }
        
        sqlite3_finalize(stmt);
        return user;
    }

    User getUserById(int id) {
        const char* sql = "SELECT id, username FROM users WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return User();
        }
        
        sqlite3_bind_int(stmt, 1, id);
        User user;
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id = sqlite3_column_int(stmt, 0);
            user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        }
        
        sqlite3_finalize(stmt);
        return user;
    }

    bool userExists(const string& username) {
        const char* sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        
        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        
        sqlite3_finalize(stmt);
        return exists;
    }

    int countUsers() {
        const char* sql = "SELECT COUNT(*) FROM users;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            stringstream ss;
            ss << "Failed to prepare search statement: " << sqlite3_errmsg(db);
            app_logger.error(ss.str());
            return 0;
        }
        
        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return count;
    }

    void createDefaultUser() {
        string username = "admin";
        string password = "admin123";
        
        if (!userExists(username)) {
            registerUser(username, password);
            app_logger.info("Created default user: " + username + "/" + password);
        }
    }
    


    ~Database() {
        close();
    }
};

//Database* Database::instance = nullptr;
inline static Database* instance = nullptr;

#endif
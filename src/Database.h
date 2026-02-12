#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include "Todo.h"
#include "Categories.h"
#include "AuthService.h"
#include "User.h"


using namespace std;

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
                tokenize='unicode61 remove_diacritics 1 categories ''L* N* Co Ps Pe Pf Pd Pc Po Sc Sm Sk So Zl Zp Zs Cc Cf''',
                prefix='2,3'
                );
            )";
        
            createTables(createFtsTable);

            int rc = sqlite3_open(path.c_str(), &db);
            if (rc != SQLITE_OK) {
                cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
                return false;
            }
            cout << "Database opened: " << path << endl;
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
                cerr << "SQL error: " << errMsg << endl;
                sqlite3_free(errMsg);
            } else {
             cout << "Tables created/checked" << endl;
            }
        }

        void checkAndPopulateFtsTable() {
        // Проверяем, существует ли таблица FTS5
        const char* check_sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='todos_fts'";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to check FTS table: " << sqlite3_errmsg(db) << endl;
            return;
        }
        
        bool fts_exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fts_exists = true;
        }
        sqlite3_finalize(stmt);
        
        if (!fts_exists) {
            cout << "FTS table not found, creating..." << endl;
            
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
                cerr << "Failed to create FTS table: " << errMsg << endl;
                sqlite3_free(errMsg);
                return;
            }
            
            cout << "FTS table created" << endl;
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
            cerr << "Error populating FTS table: " << errMsg << endl;
            sqlite3_free(errMsg);
        } else {
            cout << "FTS table populated with existing data" << endl;
        }
    }

        //_______________________________Поиск_________________________________________

    vector<Todo> searchTodos(int user_id, const string& query, bool highlight = false) {
        vector<Todo> results;
        
        if (query.empty()) {
            return results;
        }
        
        string safe_query = escapeFtsQuery(query);
        string fts_query = safe_query;
        
        string sql;
        if (highlight) {
            sql = R"(
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
                    snippet(todos_fts, 0, '<mark>', '</mark>', '…', 30) as title_snippet,
                    snippet(todos_fts, 1, '<mark>', '</mark>', '…', 40) as desc_snippet,
                    bm25(todos_fts) as relevance
                FROM todos t
                JOIN todos_fts ON t.rowid = todos_fts.rowid
                WHERE t.user_id = ?
                AND todos_fts MATCH ?
                ORDER BY relevance DESC, t.created_at DESC
                LIMIT 50
            )";
        } else {
            sql = R"(
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
                JOIN todos_fts ON t.rowid = todos_fts.rowid
                WHERE t.user_id = ?
                AND todos_fts MATCH ?
                ORDER BY relevance DESC, t.created_at DESC
                LIMIT 50
            )";
        }
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare search statement: " << sqlite3_errmsg(db) << endl;
            return results;
        }
        
        sqlite3_bind_int(stmt, 1, user_id);
        
        cout << "FTS query: '" << fts_query << "'" << endl;
        
        sqlite3_bind_text(stmt, 2, fts_query.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo todo;
            todo.id = sqlite3_column_int(stmt, 0);
            
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            todo.title = title ? title : "";
            
            const char* description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            todo.description = description ? description : "";
            
            todo.completed = sqlite3_column_int(stmt, 3) == 1;
            
            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            todo.created_at = created_at ? created_at : "";
            
            const char* due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            todo.due_date = due_date ? due_date : "";
            
            todo.priority = sqlite3_column_int(stmt, 6);
            todo.category_id = sqlite3_column_int(stmt, 7);
            todo.user_id = sqlite3_column_int(stmt, 8);
            
            if (highlight) {
                const char* title_snippet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
                const char* desc_snippet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
                
                if (title_snippet) {
                    todo.title_snippet = title_snippet;
                }
                if (desc_snippet) {
                    todo.desc_snippet = desc_snippet;
                }
                
                todo.relevance = sqlite3_column_double(stmt, 11);
            } else if (sqlite3_column_count(stmt) > 9) {
                todo.relevance = sqlite3_column_double(stmt, 9);
            }
            
            results.push_back(todo);
        }
        
        sqlite3_finalize(stmt);
        
        if (results.empty()) {
            cout << "FTS returned no results, using LIKE fallback" << endl;
            return searchTodosFallback(user_id, query);
        }
        
        return results;
    }


    vector<Todo> searchTodosFallback(int user_id, const string& query) {
        vector<Todo> results;
        
        string sql = R"(
            SELECT 
                id, title, description, completed, created_at, 
                due_date, priority, category_id, user_id
            FROM todos 
            WHERE user_id = ?
            AND (
                title LIKE ? 
                OR description LIKE ?
                OR title LIKE ? 
                OR description LIKE ?
            )
            ORDER BY created_at DESC
            LIMIT 50
        )";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Failed to prepare fallback search statement: " << sqlite3_errmsg(db) << endl;
            return results;
        }
        
        // Пробуем разные варианты поиска
        string pattern1 = "%" + query + "%";
        string pattern2 = query + "%";  // Начинается с запроса
        
        sqlite3_bind_int(stmt, 1, user_id);
        sqlite3_bind_text(stmt, 2, pattern1.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, pattern1.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, pattern2.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, pattern2.c_str(), -1, SQLITE_TRANSIENT);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo todo;
            todo.id = sqlite3_column_int(stmt, 0);
            
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            todo.title = title ? title : "";
            
            const char* description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            todo.description = description ? description : "";
            
            todo.completed = sqlite3_column_int(stmt, 3) == 1;
            
            const char* created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            todo.created_at = created_at ? created_at : "";
            
            const char* due_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            todo.due_date = due_date ? due_date : "";
            
            todo.priority = sqlite3_column_int(stmt, 6);
            todo.category_id = sqlite3_column_int(stmt, 7);
            todo.user_id = sqlite3_column_int(stmt, 8);
            
            string lower_query = query;
            string lower_title = todo.title;
            string lower_desc = todo.description;
            
            transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
            transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
            transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);
            
            // Простой расчет релевантности
            double relevance = 0.0;
            if (lower_title.find(lower_query) != string::npos) {
                relevance += 2.0;
                if (lower_title.find(lower_query) == 0) {
                    relevance += 1.0; 
                }
            }
            if (lower_desc.find(lower_query) != string::npos) {
                relevance += 1.0;
            }
            
            todo.relevance = relevance;
            
            results.push_back(todo);
        }
        
        sqlite3_finalize(stmt);
        
        // Сортируем по релевантности
        sort(results.begin(), results.end(), [](const Todo& a, const Todo& b) {
            return a.relevance > b.relevance;
        });
        
        return results;
    }


    string escapeFtsQuery(const string& query) {
        if (query.empty()) return "";
        
        string escaped;
        escaped.reserve(query.length());
        
        // FTS5 спецсимволы, которые нужно экранировать
        const string special_chars = "\"'\\*^~-:";
        
        for (char c : query) {
            if (special_chars.find(c) != string::npos) {
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

        if (!has_operators && !is_phrase && escaped.find(' ') != string::npos) {
            escaped = "\"" + escaped + "\"";
        }
        return escaped;
    }

        //_______________________________Todo__________________________________________

        Todo createTodo(const string& title, const string& description = "", const int& priority = 1, const string& due_date = "", const int& category_id = 0, int user_id =0) {
            const char* sql = "INSERT INTO todos (title, description, due_date, priority, category_id, user_id) VALUES (?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt;
            
            int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to insert: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to update: " << sqlite3_errmsg(db) << endl;
        }
        
        sqlite3_finalize(stmt);
        return success;
    }

      bool deleteTodo(int id) {
        const char* sql = "DELETE FROM todos WHERE id = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (!success) {
            cerr << "Failed to delete: " << sqlite3_errmsg(db) << endl;
        }
        
        sqlite3_finalize(stmt);
        return success;
    }

    vector<Todo> getOverdueTrue (int user_id){
        vector<Todo> todos_temp;
        const char* sql = "SELECT id, title, description, completed, created_at, due_date, priority, category_id FROM todos WHERE due_date < datetime('now') AND completed = 0 AND due_date != '' AND user_id = ? ORDER BY due_date ASC;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cout << "Todo replaced, ID: " << todo.id << endl;
        } else {
            cerr << "Failed to replace todo ID " << todo.id << ": " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
                return categories("", "", 0);
            }
            
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, color.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, user_id);
            
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                cerr << "Failed to insert: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
                cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        rc = sqlite3_step(stmt);
        bool success = (rc == SQLITE_DONE);
        
        if (!success) {
            cerr << "Failed to delete: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
            return User();
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Failed to register user: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return User();
        }
        
        int newId = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        
        cout << "User registered: " << username << " (ID: " << newId << ")" << endl;
        return getUserById(newId);
    }

    User loginUser(const string& username, const string& password) {
        const char* sql = "SELECT id, username, password_hash, salt FROM users WHERE username = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
                cout << "User authenticated: " << username << endl;
            } else {
                cout << "Invalid password for user: " << username << endl;
                user.id = 0; // Сбрасываем ID при неудачной аутентификации
            }
        } else {
            cout << "User not found: " << username << endl;
        }
        
        sqlite3_finalize(stmt);
        return user;
    }

    User getUserById(int id) {
        const char* sql = "SELECT id, username FROM users WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
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
            cout << "Created default user: " << username << "/" << password << endl;
        }
    }
    


    ~Database() {
        close();
    }
};

Database* Database::instance = nullptr;

#endif
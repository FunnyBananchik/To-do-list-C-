#ifndef TODO_H
#define TODO_H

#include <string>
#include <sstream>
#include <ctime>
#include "Logger.h"

using namespace std;

struct Todo {
    int id;
    string title;
    string description;
    bool completed;
    string created_at;
    int priority;
    string due_date;
    int category_id;
    int user_id;

    string title_snippet;  // Подсвеченный заголовок
    string desc_snippet;   // Подсвеченное описание
    double relevance;      // Релевантность поиска
    
    // Конструктор для создания новой задачи (без ID)
    Todo(string t, string d = "", int k = 1, string du = "", int cat_id = 0, int uid = 0) 
        : id(-1), title(t), description(d), completed(false), priority(k), category_id(cat_id), user_id(uid), due_date(du)
    {
        // ID будет назначен базой данных
        time_t now = time(0);
        char buffer[80];
        tm* timeinfo = localtime(&now);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        created_at = buffer;

        /*if (!du.empty()) {
            tm tm_due = {};
            istringstream ss(du);
            ss >> get_time(&tm_due, "%Y-%m-%d");
            if (!ss.fail()) {
                char due_buffer[20];
                strftime(due_buffer, sizeof(due_buffer), "%Y-%m-%d", &tm_due);
                due_date = due_buffer;  
            } else {
                due_date = ""; 
                app_logger.error("Ошибка парсинга даты: "+du);
            }
        } else due_date = ""; 
         */
    }
    
    // Конструктор по умолчанию (для БД)
    Todo() 
        : id(0), title(""), description(""), completed(false), created_at(""), priority(1), due_date(""), category_id(0), user_id(0)
    {}
    
    // Конструктор копирования
    Todo(const Todo& other) 
        : id(other.id), 
          title(other.title), 
          description(other.description), 
          completed(other.completed), 
          created_at(other.created_at),
          due_date(other.due_date),
          priority(other.priority),
          category_id(other.category_id),
          user_id(other.user_id)
    {}
    
    // Конвертация в JSON
    string to_json() const {
        stringstream json;
        json << "{";
        json << "\"id\":" << id << ",";
        json << "\"title\":\"" << escape_json(title) << "\",";
        json << "\"description\":\"" << escape_json(description) << "\",";
        json << "\"completed\":" << (completed ? "true" : "false") << ",";
        json << "\"created_at\":\"" << created_at << "\",";
        json << "\"priority\":" << priority <<",";
        json << "\"due_date\":\"" << due_date << "\",";
        json << "\"category_id\":" << category_id << ",";
        json << "\"user_id\":" << user_id;
        if (!title_snippet.empty()) {
            json << ",\"title_snippet\":\"" << escape_json(title_snippet) << "\"";
        }
        if (!desc_snippet.empty()) {
            json << ",\"desc_snippet\":\"" << escape_json(desc_snippet) << "\"";
        }
        if (relevance > 0) {
            json << ",\"relevance\":" << relevance;
        }
        json << "}";
        return json.str();
    } 
    // Статический метод для парсинга JSON
    static Todo from_json(const string& json_str) {
        Todo todo;
        
        size_t id_pos = json_str.find("\"id\":");
        if (id_pos != string::npos) {
            id_pos += 5; 
            size_t id_end = json_str.find_first_of(",}", id_pos);
            string id_str = json_str.substr(id_pos, id_end - id_pos);
            try {
                todo.id = stoi(id_str);
            } catch (...) {
                todo.id = 0;
            }
        }
        
        size_t title_pos = json_str.find("\"title\":\"");
        if (title_pos != string::npos) {
            title_pos += 9; // длина "\"title\":\""
            size_t title_end = json_str.find("\"", title_pos);
            todo.title = json_str.substr(title_pos, title_end - title_pos);
        }
        
        size_t desc_pos = json_str.find("\"description\":\"");
        if (desc_pos != string::npos) {
            desc_pos += 15; // длина "\"description\":\""
            size_t desc_end = json_str.find("\"", desc_pos);
            todo.description = json_str.substr(desc_pos, desc_end - desc_pos);
        }
        
        size_t comp_pos = json_str.find("\"completed\":");
        if (comp_pos != string::npos) {
            comp_pos += 12; // длина "\"completed\":"
            size_t comp_end = json_str.find_first_of(",}", comp_pos);
            string comp_str = json_str.substr(comp_pos, comp_end - comp_pos);
            todo.completed = (comp_str == "true");
        }
        
        size_t created_pos = json_str.find("\"created_at\":\"");
        if (created_pos != string::npos) {
            created_pos += 14; // длина "\"created_at\":\""
            size_t created_end = json_str.find("\"", created_pos);
            todo.created_at = json_str.substr(created_pos, created_end - created_pos);
        }

        size_t prior_pos = json_str.find("\"priority\":");
        if (prior_pos != string::npos) {
            prior_pos +=11;
            size_t prior_end = json_str.find(",", prior_pos);
            string priority = json_str.substr(prior_pos, prior_end - prior_pos);
            todo.priority = stoi(priority);
        }

        size_t due_pos = json_str.find("\"due_date\":\"");
        if (due_pos != string::npos) {
            due_pos +=12;
            size_t due_end = json_str.find("\"", due_pos);
            todo.due_date = json_str.substr(due_pos, due_end - due_pos);
        }

        size_t cat_id_pos = json_str.find("\"category_id\":");
        if (cat_id_pos != string::npos) {
            cat_id_pos += 14;
            size_t cat_id_end = json_str.find_first_of(",}", cat_id_pos);
            string cat_id_temp = json_str.substr(cat_id_pos, cat_id_end - cat_id_pos);
            
            if (!cat_id_temp.empty() && cat_id_temp != "null") {
                try {
                    if (cat_id_temp.front() == '"' && cat_id_temp.back() == '"') {
                        cat_id_temp = cat_id_temp.substr(1, cat_id_temp.length() - 2);
                    }
                    if (!cat_id_temp.empty()) {
                        todo.category_id = stoi(cat_id_temp);
                    }
                } catch (const exception& e) {
                    stringstream ss;
                    ss << "Error parsing category_id: " << cat_id_temp << " - " << e.what();
                    app_logger.error(ss.str());
                    todo.category_id = 0;
                }
            }
        }

        size_t user_id_pos = json_str.find("\"user_id\":");
            if (user_id_pos != string::npos) {
                user_id_pos += 10;
                size_t user_id_end = json_str.find_first_of(",}", user_id_pos);
                string user_id_temp = json_str.substr(user_id_pos, user_id_end - user_id_pos);
                
                if (!user_id_temp.empty() && user_id_temp != "null") {
                    try {
                        if (user_id_temp.front() == '"' && user_id_temp.back() == '"') {
                            user_id_temp = user_id_temp.substr(1, user_id_temp.length() - 2);
                        }
                        if (!user_id_temp.empty()) {
                            todo.user_id = stoi(user_id_temp);
                        }
                    } catch (const exception& e) {
                        stringstream ss;
                        ss << "Error parsing user_id: " << user_id_temp << " - " << e.what();
                        app_logger.error(ss.str());
                        todo.user_id = 0;
                    }
                }
            }

        return todo;
    }
    
private:
    string escape_json(const string& s) const {
        string result;
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        return result;
    }
};

#endif
#include "../httplib.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <fstream>
#include <sys/stat.h>  
#include <unistd.h>     
#include "Todo.h"
#include "Database.h"
#include "Categories.h"
#include "User.h"
#include "AuthService.h"
#include "AuthMiddleware.h"
#include "AuthUtils.h"

using namespace std;

Database* db = nullptr;


bool file_exists(const string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}


string get_current_directory() {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return string(buffer);
    }
    return "Unknown";
}

string todos_to_json(vector<Todo>& todos) {
        stringstream json;
        json << "[";
        for (size_t i = 0; i < todos.size(); ++i) {
            json << todos[i].to_json();
            if (i != todos.size() - 1) json << ",";
        }
        json << "]";
        return json.str();
    }

string cat_to_json(vector<categories>& cat) {
        stringstream json;
        json << "[";
        for (size_t i = 0; i < cat.size(); ++i) {
            json << cat[i].to_json();
            if (i != cat.size() - 1) json << ",";
        }
        json << "]";
        return json.str();
    }



string read_html_file(const string& filename) {
   
    vector<string> possible_paths = {
        filename,                           
        "src/" + filename,                  
        "../src/" + filename,               
        "../../src/" + filename             
    };
    
    for (const auto& path : possible_paths) {
        if (file_exists(path)) {
            ifstream file(path);
            if (file.is_open()) {
                stringstream buffer;
                buffer << file.rdbuf();
                cout << "✅ Loaded frontend from: " << path << endl;
                return buffer.str();
            }
        }
    }
    
    // error HTML
    string error_html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Error - Frontend not found</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 50px; text-align: center; }
            .error { color: #d32f2f; background: #ffebee; padding: 20px; border-radius: 10px; }
        </style>
    </head>
    <body>
        <div class="error">
            <h1> Frontend file not found!</h1>
            <p>Looking for: )" + filename + R"(</p>
            <p>Current directory: )" + get_current_directory() + R"(</p>
        </div>
    </body>
    </html>
    )";
    
    cerr << " Could not load " << filename << ". Tried paths:" << endl;
    for (const auto& path : possible_paths) {
        cerr << "   - " << path << endl;
    }
    
    return error_html;
}

map<string, string> parse_simple_json(const string& json_str) {
    map<string, string> result;
    
    string content = json_str;
    if (content.front() == '{') content = content.substr(1);
    if (content.back() == '}') content.pop_back();
    
    stringstream ss(content);
    string item;
    
    while (getline(ss, item, ',')) {
        size_t colon_pos = item.find(':');
        if (colon_pos != string::npos) {
            string key = item.substr(0, colon_pos);
            string value = item.substr(colon_pos + 1);
            
            
            key.erase(remove(key.begin(), key.end(), '"'), key.end());
            key.erase(remove(key.begin(), key.end(), ' '), key.end());
            
            value.erase(remove(value.begin(), value.end(), '"'), value.end());
            if (!value.empty() && value.front() == ' ') {
                value = value.substr(1);
            }
            
            result[key] = value;
        }
    }
    
    return result;
}

// Функция для проверки аутентификации и установки ошибки
bool require_auth_and_get_user(const string& auth_header, 
                               int& user_id, 
                               string& username, 
                               httplib::Response& res) {
    string error;
    if (!AuthUtils::checkAuth(auth_header, user_id, username, error)) {
        res.status = 401;
        res.set_content(AuthUtils::authError(error), "application/json");
        return false;
    }
    return true;
}

// Шаблонная функция для защищенных endpoints
template<typename Func>
void protected_endpoint(const httplib::Request& req, 
                       httplib::Response& res, 
                       Database* db,
                       Func handler) {
    auto auth_header = req.get_header_value("Authorization");
    
    int user_id;
    string username;
    
    if (!require_auth_and_get_user(auth_header, user_id, username, res)) {
        return;
    }
    
    handler(req, res, db, user_id, username);
}


int main() {
    httplib::Server server;
    
    db = Database::getInstance();
    

    string db_path = "todo_app.db";  
    
    if (!db->open(db_path)) {
        cerr << "Failed to open database! Exiting..." << endl;
        return 1;
    }
    
    // CORS setup
    server.set_pre_routing_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    //OPTIONS (для CORS)
    server.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
    });
    
    // 1. HOME PAGE
    server.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        string html = read_html_file("frontend.html");
        res.set_content(html, "text/html");
    });

    // 2. APP PAGE
    server.Get("/app", [](const httplib::Request& req, httplib::Response& res) {
        // Проверка токенa из куки или localStorage
        auto auth_header = req.get_header_value("Authorization");
        
        if (auth_header.empty()) {
            auto cookies = req.headers.find("Cookie");
            if (cookies != req.headers.end()) {
                
                string cookie_str = cookies->second;
                size_t token_pos = cookie_str.find("auth_token=");
                if (token_pos != string::npos) {
                    token_pos += 11;
                    size_t token_end = cookie_str.find(";", token_pos);
                    string token = cookie_str.substr(token_pos, token_end - token_pos);
                    auth_header = "Bearer " + token;
                }
            }
        }
        int user_id;
        string username;
        
        if (!AuthMiddleware::requireAuth(auth_header, user_id, username)) {
            res.set_redirect("/");
            return;
        }
        string html = read_html_file("frontend.html");
        res.set_content(html, "text/html");
    });


    // 3. Регистрация
    server.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        cout << "Registration request" << endl;
        
        auto params = parse_simple_json(req.body);
        string username = params["username"];
        string password = params["password"];
        
        if (username.empty() || password.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Username and password are required\"}", "application/json");
            return;
        }
        
        if (!AuthMiddleware::validatePasswordStrength(password)) {
            res.status = 400;
            res.set_content("{\"error\":\"Password must be at least 6 characters with letters and numbers\"}", "application/json");
            return;
        }
        
        if (db->userExists(username)) {
            res.status = 409;
            res.set_content("{\"error\":\"Username already exists\"}", "application/json");
            return;
        }
        
        User new_user = db->registerUser(username, password);
        
        if (new_user.id == 0) {
            res.status = 500;
            res.set_content("{\"error\":\"Registration failed\"}", "application/json");
            return;
        }
        
        // Генерация токенa
        string token = AuthService::generateToken(new_user.id, new_user.username);
        
        cout << " User registered: " << username << " (ID: " << new_user.id << ")" << endl;
        res.set_content(new_user.to_json_with_token(token), "application/json");
        res.status = 201;
    });

    //4. LOGIN

    server.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        cout << "Login request" << endl;
        
        auto params = parse_simple_json(req.body);
        string username = params["username"];
        string password = params["password"];
        
        if (username.empty() || password.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"Username and password are required\"}", "application/json");
            return;
        }
        
        User user = db->loginUser(username, password);
        
        if (user.id == 0) {
            res.status = 401;
            res.set_content("{\"error\":\"Invalid username or password\"}", "application/json");
            return;
        }
        
        string token = AuthService::generateToken(user.id, user.username);
        
        res.set_header("Set-Cookie", 
            "auth_token=" + token + "; HttpOnly; Path=/; Max-Age=" + to_string(24 * 3600));
        
        cout << "User logged in: " << username << " (ID: " << user.id << ")" << endl;
        res.set_content(user.to_json_with_token(token), "application/json");
    });

    // 5. LOGOUT
    server.Post("/api/logout", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Set-Cookie", "auth_token=; HttpOnly; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        res.set_content("{\"message\":\"Logout successful\"}", "application/json");
    });
    
    // 6. VALIDATE TOKEN
    server.Get("/api/validate", [](const httplib::Request& req, httplib::Response& res) {
        auto auth_header = req.get_header_value("Authorization");
        
        int user_id;
        string username;
        
        if (AuthMiddleware::requireAuth(auth_header, user_id, username)) {
            stringstream json;
            json << "{";
            json << "\"valid\":true,";
            json << "\"user_id\":" << user_id << ",";
            json << "\"username\":\"" << username << "\"";
            json << "}";
            res.set_content(json.str(), "application/json");
        } else {
            res.status = 401;
            res.set_content("{\"valid\":false,\"error\":\"Invalid token\"}", "application/json");
        }
    });
 
    
       // 7. USER PROFILE
    server.Get("/api/profile", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db, 
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
                User user = db->getUserById(user_id);
                if (user.id == 0) {
                    res.status = 404;
                    res.set_content("{\"error\":\"User not found\"}", "application/json");
                    return;
                }
                res.set_content(user.to_json(), "application/json");
            }
        );
    });
        //________________________________Методы для todo_____________________________________
    
    // 1. GET ALL TASKS 
    server.Get("/api/todos", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
                auto todos = db->getUserTodos(user_id);
        string sort_by = req.get_param_value("sort_by");
        string order = req.get_param_value("order");
        string filter = req.get_param_value("overdue");
        string category_ids_str  = req.get_param_value("category_ids");

        vector<Todo> sorted_todos;
        if (filter == "true") sorted_todos = db->getOverdueTrue(user_id);
        else if (!category_ids_str.empty()) {
                vector<int> category_ids;
                istringstream iss(category_ids_str);
                string id;
                
                while (getline(iss, id, ',')) {
                    try {
                        category_ids.push_back(stoi(id));
                    } catch (...) {
                
                    }
                }
                
                if (!category_ids.empty()) {
                    sorted_todos = db->getTodosByCategories(category_ids, user_id);
                } else {
                   
                    sorted_todos = db->getUserTodos(user_id);
                }
            } else {
            
            sorted_todos = db->getUserTodos(user_id);
            }
        // Сортируем если указаны параметры
         if (!sort_by.empty()) {
              if (sort_by == "title") {
                 sort(sorted_todos.begin(), sorted_todos.end(), 
                     [order](const Todo& a, const Todo& b) {
                          if (order == "desc") return b.title < a.title;
                         return a.title < b.title; 
                         });
                }
                else if (sort_by == "completed") {
                    sort(sorted_todos.begin(), sorted_todos.end(), 
                        [order](const Todo& a, const Todo& b) {
                         if (order == "desc") return b.completed < a.completed;
                         return a.completed < b.completed;
                        });
             }
             else if (sort_by == "priority") {
                 sort(sorted_todos.begin(), sorted_todos.end(), 
                      [order](const Todo& a, const Todo& b) {
                            if (order == "desc") return b.priority < a.priority;
                           return a.priority < b.priority;
                     });
                }
                else if (sort_by == "id") {
                 sort(sorted_todos.begin(), sorted_todos.end(), 
                      [order](const Todo& a, const Todo& b) {
                            if (order == "desc") return b.id < a.id;
                           return a.id < b.id;
                        });
             }
         }
        res.set_content(todos_to_json(sorted_todos), "application/json");
        });
    });
    
     // 2. CREATE TODO
    server.Post("/api/todos", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
            auto params = parse_simple_json(req.body);
            
            string title = params["title"];
            string description = params["description"];
            string due_date = params["due_date"];
            int priority = params["priority"].empty() ? 1 : stoi(params["priority"]);
            int category_id = params["category_id"].empty() ? 0 : stoi(params["category_id"]);
            
            if (title.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"Title is required\"}", "application/json");
                return;
            }
            
            Todo created = db->createTodo(title, description, priority, due_date, category_id, user_id);
            
            if (created.id == 0) {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to create todo\"}", "application/json");
                return;
            }
            
            cout << "Todo created for user '" << username << "', ID: " << created.id << endl;
            res.set_content(created.to_json(), "application/json");
            res.status = 201;
        });
    });
    
    // 3. GET TASK BY ID
    server.Get(R"(/api/todos/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        int id = stoi(req.matches[1]);
        
        Todo todo = db->getTodoById(id, user_id);
        
        if (todo.id == 0) {
            res.set_content("{\"error\":\"Todo not found\"}", "application/json");
            res.status = 404;
            return;
        }
        
        res.set_content(todo.to_json(), "application/json");
         });
    });
    
    // 4. UPDATE TASK
    server.Put(R"(/api/todos/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
    protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        int id = stoi(req.matches[1]);
    
    try {
        
        Todo updatedTodo = Todo::from_json(req.body);
        updatedTodo.id = id; 
        updatedTodo.user_id = user_id;
        // Логируем 
        cout << "🔍 Парсинг JSON:" << endl;
            cout << "   ID: " << updatedTodo.id << endl;
            cout << "   Title: " << updatedTodo.title << endl;
            cout << "   Description: " << updatedTodo.description << endl;
            cout << "   Completed: " << (updatedTodo.completed ? "true" : "false") << endl;
            cout << "   Priority: " << updatedTodo.priority << endl;
            cout << "   Due_date: " << updatedTodo.due_date << endl;
            cout << "   Category_id: " << updatedTodo.category_id << endl;
            cout << "   User_id: " << updatedTodo.user_id << endl;

        if (db->replaceTodo(updatedTodo, user_id)) {
            Todo todo = db->getTodoById(id, user_id);
            if (todo.id != 0) {
                res.set_content(todo.to_json(), "application/json");
                cout << "Successfully updated todo ID: " << id << endl;
            } else {
                res.set_content("{\"error\":\"Todo not found after update\"}", "application/json");
                res.status = 404;
                cerr << "Todo not found after update, ID: " << id << endl;
            }
        } else {
            res.set_content("{\"error\":\"Update failed\"}", "application/json");
            res.status = 500;
            cerr << "Update failed for todo ID: " << id << endl;
        }
        } catch (const exception& e) {
        res.set_content(string("{\"error\":\"") + e.what() + "\"}", "application/json");
        res.status = 400;
        cerr << "JSON parsing error: " << e.what() << endl;
        }
        });
    });


    //5, UPDATE COMPLITED
    server.Patch(R"(/api/todos/(\d+)/toggle)", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        int id = stoi(req.matches[1]);
        
        Todo todo = db->getTodoById(id, user_id);
        if (todo.id == 0) {
            res.set_content("{\"error\":\"Todo not found\"}", "application/json");
            res.status = 404;
            return;
        }
        
        todo.completed = !todo.completed;
        
        if (db->updateTodo(todo)) {
            
            Todo updatedTodo = db->getTodoById(id, user_id);
            res.set_content(updatedTodo.to_json(), "application/json");
        } else {
            res.set_content("{\"error\":\"Toggle failed\"}", "application/json");
            res.status = 500;
        }
        });
    });
    
    // 6. DELETE TASK
    server.Delete(R"(/api/todos/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        int id = stoi(req.matches[1]);
        
        if (db->deleteTodo(id)) {
            res.status = 204; // No Content
        } else {
            res.set_content("{\"error\":\"Todo not found\"}", "application/json");
            res.status = 404;
        }
    });
});

    
    // 7. STATISTICS
    server.Get("/api/stats", [](const httplib::Request& req, httplib::Response& res) {
         protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        auto stats = db->getStats(user_id);
        
        stringstream json;
        json << "{";
        json << "\"total\":" << stats.total << ",";
        json << "\"completed\":" << stats.completed << ",";
        json << "\"pending\":" << stats.pending;
        json << "}";
        
        res.set_content(json.str(), "application/json");
         });
    });
    
    // 8. HEALTH CHECK
    server.Get("/health", [](const httplib::Request& req, httplib::Response& res) {
         protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        stringstream json;
        json << "{";
        json << "\"status\":\"healthy\",";
        json << "\"database\":\"connected\",";
        auto stats = db->getStats(user_id);
        json << "\"todos_count\":" << stats.total << ",";
        json << "\"timestamp\":" << time(0);
        json << "}";
        
        res.set_content(json.str(), "application/json");
         });
    });
    
    // 9. SERVER INFO
    server.Get("/api/info", [](const httplib::Request& req, httplib::Response& res) {
         protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        stringstream json;
        json << "{";
        json << "\"name\":\"Todo Backend Server\",";
        json << "\"version\":\"1.0\",";
        json << "\"language\":\"C++\",";
        json << "\"framework\":\"cpp-httplib\",";
        json << "\"endpoints\":[";
        json << "\"/api/todos\",";
        json << "\"/api/todos/{id}\",";
        json << "\"/api/stats\",";
        json << "\"/health\",";
        json << "\"/api/info\"";
        json << "]";
        json << "}";
        
        res.set_content(json.str(), "application/json");
         });
    });

    //10. GET EXPIRATION TODOS
    server.Get("/api/todos/expiration" , [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        auto overdue_todos = db->getExpirationTodo(user_id);
        res.set_content(todos_to_json(overdue_todos), "application/json");
        });
    });

    //____________________________Методы для категорий____________________________________

    //1. GET CATEGORIES
    server.Get("/api/categories", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        auto list_cat = db->getAllCat(user_id);
        res.set_content(cat_to_json(list_cat), "application/json");
        });
    });

    //2. CREATE CATEGORIES
    server.Post("/api/categories", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        string body = req.body;
        string name = "";
        string color = "";

        size_t name_pos = body.find("\"name\":\"");
        if (name_pos != string::npos) {
            name_pos += 8; 
            size_t name_end = body.find("\"", name_pos);
            name = body.substr(name_pos, name_end - name_pos);
        }
        
        size_t color_pos = body.find("\"color\":\"");
        if (color_pos != string::npos) {
            color_pos += 9; 
            size_t color_end = body.find("\"", color_pos);
            color = body.substr(color_pos, color_end - color_pos);
        }

        if (name.empty() || color.empty()) {
            res.set_content("{\"error\":\"Name or color is required\"}", "application/json");
            res.status = 400;
            return;
        }        

        categories created = db->createCat(name, color, user_id);
        
        if (created.id == 0) {
            res.set_content("{\"error\":\"Failed to create cat\"}", "application/json");
            res.status = 500;
            return;
        }
        
        res.set_content(created.to_json(), "application/json");
        res.status = 201;
    });
    });

    //3. DELETE CATEGORIES
    server.Delete(R"(/api/categories/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        protected_endpoint(req, res, db,
            [](const httplib::Request& req, httplib::Response& res, Database* db, int user_id, string username) {
        int id = stoi(req.matches[1]);
        
        if (db->deleteCat(id)) {
            res.status = 204; // No Content
        } else {
            res.set_content("{\"error\":\"Cat not found\"}", "application/json");
            res.status = 404;
        }
    });
});

    
    cout << "Server running on http://localhost:8080" << endl << endl;
    cout << "Press Ctrl+C to stop" << endl;
    cout << "======================================" << endl;
    
    server.listen("0.0.0.0", 8080);
    
    return 0;
}
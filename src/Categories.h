#ifndef CATEGORIES_H
#define CATEGORIES_H

#include <string>
#include <sstream>
#include "Logger.h"

using namespace std;

struct categories {
    int id;
    string name;
    string color;
    int user_id;
    
    categories (string n, string c, int uid): id(-1), name(n), color(c), user_id(uid) {}
    categories(): id(0), name(""), color(""), user_id(0) {}
    categories(const categories& other) 
        : id(other.id), 
          name(other.name), 
          color(other.color),
          user_id(other.user_id)
    {}

    string to_json() const {
        stringstream json;
        json << "{";
        json << "\"id\":" << id << ",";
        json << "\"name\":\"" << escape_json(name) << "\",";
        json << "\"color\":\"" << escape_json(color) << "\",";
        json << "\"user_id\":" << user_id;
        json << "}";
        return json.str();
    } 

    static categories cat_from_json(const string& json_str) {
        categories cat;
        
        size_t id_pos = json_str.find("\"id\":");
        if (id_pos != string::npos) {
            id_pos += 5; // длина "\"id\":"
            size_t id_end = json_str.find_first_of(",}", id_pos);
            string id_str = json_str.substr(id_pos, id_end - id_pos);
            try {
                cat.id = stoi(id_str);
            } catch (...) {
                cat.id = 0;
            }
        }
        
        size_t name_pos = json_str.find("\"name\":\"");
        if (name_pos != string::npos) {
            name_pos += 8; // длина "\"name\":\""
            size_t name_end = json_str.find("\"", name_pos);
            cat.name = json_str.substr(name_pos, name_end - name_pos);
        }
        
        size_t color_pos = json_str.find("\"color\":\"");
        if (color_pos != string::npos) {
            color_pos += 9; // длина "\"color\":\""
            size_t color_end = json_str.find("\"", color_pos);
            cat.color = json_str.substr(color_pos, color_end - color_pos);
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
                            cat.user_id = stoi(user_id_temp);
                        }
                    } catch (const exception& e) {
                        stringstream ss;
                        ss << "Error parsing user_id: " << user_id_temp << " - " << e.what();
                        app_logger.error(ss.str());
                        cat.user_id = 0;
                    }
                }
            }

        return cat;
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

#endif CATEGORIES_H
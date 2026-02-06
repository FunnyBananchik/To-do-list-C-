#ifndef USER_H
#define USER_H

#include <string>
#include <sstream>
#include <ctime>

using namespace std;

struct User
{
    int id;
    string username;
    string password_hash;
    string salt;
    string created_at;

    User() : id(0), username(""), password_hash(""), salt("") {}
    
    User(string uname, string pwd_hash, string s) 
        : id(0), username(uname), password_hash(pwd_hash), salt(s)
    {
        time_t now = time(0);
        char buffer[80];
        tm* timeinfo = localtime(&now);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        created_at = buffer;
    }

    string to_json() const {
        stringstream json;
        json << "{";
        json << "\"id\":" << id << ",";
        json << "\"username\":\"" << escape_json(username) << "\",";
        json << "\"created_at\":\"" << created_at << "\"";
        json << "}";
        return json.str();
    }
    
    string to_json_with_token(const string& token) const {
        stringstream json;
        json << "{";
        json << "\"id\":" << id << ",";
        json << "\"username\":\"" << escape_json(username) << "\",";
        json << "\"created_at\":\"" << created_at << "\",";
        json << "\"token\":\"" << escape_json(token) << "\"";
        json << "}";
        return json.str();
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





#endif USER_H
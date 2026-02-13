#ifndef LOGGER_H
#define LOGGER_H


#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

enum LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
private:
    std::ofstream file;
    std::mutex mtx;
    LogLevel min_level;
    
    std::string levelToString(LogLevel level) {
        switch(level) {
            case DEBUG: return "DEBUG";
            case INFO:  return "INFO ";
            case WARN:  return "WARN ";
            case ERROR: return "ERROR";
            default:    return "UNKNOWN";
        }
    }
    
    std::string currentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
public:
    Logger(const std::string& filename = "logs/todo_app.log", LogLevel level = INFO) 
        : min_level(level) {
        
        size_t pos = filename.find_last_of('/');
        if (pos != std::string::npos) {
            std::string dir = filename.substr(0, pos);
            
            #ifdef _WIN32
                std::string cmd = "if not exist " + dir + " mkdir " + dir;
            #else
                std::string cmd = "mkdir -p " + dir;
            #endif
            
            int result = system(cmd.c_str());
            if (result != 0) {
                std::cerr << "Warning: Could not create directory: " << dir << std::endl;
            }
        }
        
        file.open(filename, std::ios::app);
        
        if (!file.is_open()) {
            std::cerr << "Cannot open log file!" << std::endl;
            std::cerr << "Filename: " << filename << std::endl;
        } 
    }
    
    ~Logger() {
        if (file.is_open()) file.close();
    }
    
    void log(LogLevel level, const std::string& message) {
        if (level < min_level) return;
        std::lock_guard<std::mutex> lock(mtx);
        if (file.is_open()) {
            file << "[" << currentTimestamp() << "] "
                 << "[" << levelToString(level) << "] "
                 << message << std::endl;
            file.flush();
        }
    }
    
    void debug(const std::string& msg) { log(DEBUG, msg); }
    void info(const std::string& msg)  { log(INFO,  msg); }
    void warn(const std::string& msg)  { log(WARN,  msg); }
    void error(const std::string& msg) { log(ERROR, msg); }
};

extern Logger app_logger;

#endif
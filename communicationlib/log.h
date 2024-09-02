#pragma once

#include <iomanip>
#include <iostream>
#include <utility>

#include "format.h"

#define log_error(...) Logger::log("ERROR", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_warn(...) Logger::log("WARN", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_info(...) Logger::log("INFO", __FILE__, __LINE__, ##__VA_ARGS__)

class Logger {
public:
    template<typename ...Args>
    static void log(const char* level, const char* file, int line, Args && ...args)
    {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        (std::cout << std::put_time(&tm, "%H:%M:%S ") << format("%s [%s:%i]: ", level, file, line) << ... << args) << std::endl;
    }
};

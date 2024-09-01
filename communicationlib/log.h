#pragma once

#include <format>
#include <iomanip>
#include <iostream>
#include <utility>

#define log_error(...) log("ERROR", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_warn(...) log("WARN", __FILE__, __LINE__, ##__VA_ARGS__)
#define log_info(...) log("INFO", __FILE__, __LINE__, ##__VA_ARGS__)

template<typename ...Args>
void log(const char* level, const char* file, int line, Args && ...args)
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    (std::cout << std::put_time(&tm, "%H:%M:%S ") << std::format("{} [{}:{}]: ", level, file, line) << ... << args) << std::endl;
}

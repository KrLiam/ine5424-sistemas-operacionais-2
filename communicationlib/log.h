#pragma once

#include <format>
#include <iostream>
#include <utility>

#define log_error(...) log("ERROR", __TIME__, __FILE__, __LINE__, ##__VA_ARGS__)
#define log_warn(...) log("WARN", __TIME__, __FILE__, __LINE__, ##__VA_ARGS__)
#define log_info(...) log("INFO", __TIME__, __FILE__, __LINE__, ##__VA_ARGS__)

template<typename ...Args>
void log(const char* level, const char* time, const char* file, int line, Args && ...args)
{
    (std::cout << std::format("{} {} [{}:{}]: ", time, level, file, line) << ... << args) << std::endl;
}

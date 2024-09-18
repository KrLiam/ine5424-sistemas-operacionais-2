#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>

#include "utils/format.h"

#if !defined(LOG_LEVEL)
#define LOG_LEVEL 2
#endif

#if LOG_LEVEL <= 4
#define log_error(...) Logger::log("ERROR", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_error(...)
#endif

#if LOG_LEVEL <= 3
#define log_warn(...) Logger::log("WARN", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_warn(...)
#endif

#if LOG_LEVEL <= 2
#define log_info(...) Logger::log("INFO", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_info(...)
#endif

#if LOG_LEVEL <= 1
#define log_debug(...) Logger::log("DEBUG", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_debug(...)
#endif

#if LOG_LEVEL <= 0
#define log_trace(...) Logger::log("TRACE", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_trace(...)
#endif

static std::mutex log_mutex;

class Logger
{
public:
    template <typename... Args>
    static void log(const char *level, const char *file, int line, Args &&...args)
    {
        log_mutex.lock();
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);

        std::ostringstream oss;
        (oss << std::put_time(&tm, "%H:%M:%S ") << format("%s [%s:%i]: ", level, file, line) << ... << args) << std::endl;
        std::cout << oss.str();

        log_mutex.unlock();
    }
};

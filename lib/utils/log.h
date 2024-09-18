#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>

#include "utils/format.h"
#include "utils/ansi.h"

#ifndef LOG_FILES
#define LOG_FILES true
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif


#define LABEL_ERROR RED "ERROR" COLOR_RESET
#define LABEL_WARN YELLOW "WARN" COLOR_RESET
#define LABEL_INFO GREEN "INFO" COLOR_RESET
#define LABEL_DEBUG BLUE "DEBUG" COLOR_RESET
#define LABEL_TRACE CYAN "TRACE" COLOR_RESET


#if LOG_LEVEL <= 4
#define log_error(...) Logger::log(LABEL_ERROR, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_error(...)
#endif

#if LOG_LEVEL <= 3
#define log_warn(...) Logger::log(LABEL_WARN, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_warn(...)
#endif

#if LOG_LEVEL <= 2
#define log_info(...) Logger::log(LABEL_INFO, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_info(...)
#endif

#if LOG_LEVEL <= 1
#define log_debug(...) Logger::log(LABEL_DEBUG, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_debug(...)
#endif

#if LOG_LEVEL <= 0
#define log_trace(...) Logger::log(LABEL_TRACE, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_trace(...)
#endif


#define IGNORE_UNUSED(x) (void)x


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

        (oss
        << std::put_time(&tm, BOLD_H_WHITE "%H:%M:%S" COLOR_RESET)
        << format(" %s", level)
        #if LOG_FILES
        << format(H_BLACK " [%s:%i]" COLOR_RESET, file, line)
        #endif
        << ": "
        << ...
        << args)
        << std::endl;

        std::cout << oss.str();

        log_mutex.unlock();

        IGNORE_UNUSED(file);
        IGNORE_UNUSED(line);
    }
};

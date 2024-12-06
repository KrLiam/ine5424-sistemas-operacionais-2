#pragma once

#include <iomanip>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <thread>
#include <unordered_map>
#include <memory>
#include <fstream>

#include "utils/format.h"
#include "utils/ansi.h"
#include "utils/reader.h"


namespace LogLevel {
    enum Type : uint8_t {
        ERROR = 4,
        WARN = 3,
        INFO = 2,
        DEBUG = 1,
        TRACE = 0
    };

    Type parse(Reader& reader);
    Type parse(const std::string& level_str);


    std::string to_string(Type level);
    static std::unordered_map<Type, const char*> label_color = {
        {ERROR, RED},
        {WARN, YELLOW},
        {INFO, GREEN},
        {DEBUG, BLUE},
        {TRACE, CYAN}
    };
    const char* to_color(Type level);
}


#ifndef LOG_LEVEL
#define LOG_LEVEL LogLevel::TRACE
#endif


#if LOG_LEVEL <= 4
#define log_error(...) Logger::log(LogLevel::ERROR, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_error(...)
#endif

#if LOG_LEVEL <= 3
#define log_warn(...) Logger::log(LogLevel::WARN, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_warn(...)
#endif

#if LOG_LEVEL <= 2
#define log_info(...) Logger::log(LogLevel::INFO, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_info(...)
#endif

#if LOG_LEVEL <= 1
#define log_debug(...) Logger::log(LogLevel::DEBUG, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_debug(...)
#endif

#if LOG_LEVEL <= 0
#define log_trace(...) Logger::log(LogLevel::TRACE, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define log_trace(...)
#endif


#define log_print(...) Logger::print("", ##__VA_ARGS__ )


int get_thread_id();


static std::mutex log_mutex;

extern std::ofstream log_open_file;
extern std::ostream* log_out;
extern std::string prefix;
extern bool log_colored;
extern bool log_show_files;
extern LogLevel::Type log_level;

class Logger
{
public:
    static void set_prefix(const std::string& value) {
        prefix = value;
    }

    static void set_colored(bool value) {
        log_colored = value;
    }

    static void set_level(LogLevel::Type value) {
        log_level = value;
    }

    static void show_files(bool value) {
        log_show_files = value;
    }

    static void set_output_file(const std::string& path) {
        log_open_file = std::ofstream(path);
        log_out = &log_open_file;
        // std::cout.rdbuf(log_out.rdbuf());
    }   

    template <typename... Args>
    static void log(LogLevel::Type level, [[maybe_unused]] const char *file, [[maybe_unused]] int line, Args &&...args)
    {
        if (level < log_level) return;

        log_mutex.lock();

        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&timer);

        int thread_id = get_thread_id();

        std::string file_str = log_show_files ? format(" [%s:%i]", file, line) : "";
        std::string level_str = LogLevel::to_string(level);

        std::ostringstream oss;

        if (log_colored) {
            (oss
            << prefix
            << std::put_time(&tm, BOLD_H_WHITE "%H:%M:%S" COLOR_RESET) << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << format(" %s%s" COLOR_RESET, LogLevel::to_color(level), level_str.c_str())
            << format(H_BLACK "%s" COLOR_RESET, file_str.c_str())
            << format(H_BLACK " (%u)" COLOR_RESET, thread_id)
            << ": "
            << ...
            << args)
            << std::endl;
        }
        else {
            (oss
            << prefix
            << std::put_time(&tm, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << format(" %s", level_str.c_str())
            << file_str
            << format(" (%u)", thread_id)
            << ": "
            << ...
            << args)
            << std::endl;
        }

        *log_out << oss.str() << std::flush;

        log_mutex.unlock();
    }

    template <typename... Args>
    static void print([[maybe_unused]] std::string _, Args &&...args)
    {
        log_mutex.lock();

        std::ostringstream oss;

        (oss << prefix << ... << args) << std::endl;
        *log_out << oss.str() << std::flush;

        log_mutex.unlock();
    }
};

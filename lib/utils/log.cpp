#include <thread>
#include <unordered_map>

#include "log.h"

std::ofstream log_out;
std::string prefix;
bool log_colored = true;
bool log_show_files = true;
LogLevel::Type log_level = LogLevel::INFO;

std::unordered_map<std::thread::id, int> id_map;
int counter = 0;

int get_thread_id() {
    std::thread::id id = std::this_thread::get_id();

    if (!id_map.contains(id)) {
        id_map.emplace(id, counter++);
    }

    return id_map.at(id);
}

LogLevel::Type LogLevel::parse(const std::string& level_str) {
    Reader reader(level_str);
    return parse(reader);
}
LogLevel::Type LogLevel::parse(Reader& reader) {
    std::string level_str = reader.read_word();
    std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::tolower);

    if (level_str == "error") return ERROR;
    if (level_str == "warn") return WARN;
    if (level_str == "info") return INFO;
    if (level_str == "debug") return DEBUG;
    if (level_str == "trace") return TRACE;

    throw parse_error(format("Invalid log level '%s'", level_str.c_str()));
}

std::string LogLevel::to_string(Type level) {
    if (level == ERROR) return "ERROR";
    if (level == WARN) return "WARN";
    if (level == INFO) return "INFO";
    if (level == DEBUG) return "DEBUG";
    return "TRACE";
}

const char* LogLevel::to_color(Type level) {
    return label_color.at(level);
}
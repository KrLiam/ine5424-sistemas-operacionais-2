#pragma once

#include <string>
#include <vector>
#include <optional>

#include "utils/reader.h"
#include "utils/log.h"

#include "command.h"


struct Arguments {
    std::string program_name;
    std::string node_id;
    std::optional<SocketAddress> address;
    std::vector<std::shared_ptr<Command>> send_commands;

    bool verbose = false;
    bool test = false;
    bool benchmark = false;
    std::string case_path;

    LogLevel::Type log_level  = LogLevel::INFO;
    bool specified_log_tail = false;
    bool log_tail = true;
};

Arguments parse_arguments(int argc, char* argv[]);

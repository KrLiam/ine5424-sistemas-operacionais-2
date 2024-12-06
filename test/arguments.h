#pragma once

#include <string>
#include <vector>
#include <optional>

#include "utils/reader.h"
#include "utils/log.h"
#include "core/message.h"

#include "command.h"


struct Arguments {
    std::string program_name;
    std::string node_id;
    std::optional<SocketAddress> address;
    std::vector<std::shared_ptr<Command>> send_commands;

    LogLevel::Type log_level  = LogLevel::INFO;
    bool verbose = false;

    bool test = false;
    std::string case_path;
    bool specified_log_tail = false;
    bool log_tail = true;

    bool benchmark = false;
    uint32_t num_groups = 3;
    uint32_t num_nodes = 10;
    uint32_t bytes_per_node = 100*1024*1024;
    uint32_t node_interval = 0;
    uint32_t max_message_size = Message::MAX_SIZE;
};

Arguments parse_arguments(int argc, char* argv[]);

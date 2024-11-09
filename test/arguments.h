#pragma once

#include <string>
#include <vector>

#include "utils/reader.h"

#include "command.h"


struct Arguments {
    std::string node_id;
    std::vector<int> faults;
    std::vector<std::shared_ptr<Command>> send_commands;

    bool test = false;
    std::string case_path;
};

std::vector<int> parse_fault_list(Reader& reader);

Arguments parse_arguments(int argc, char* argv[]);

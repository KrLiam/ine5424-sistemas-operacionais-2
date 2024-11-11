#pragma once

#include <string>
#include <vector>

#include "utils/reader.h"

#include "command.h"


struct Arguments {
    std::string node_id;
    std::vector<std::shared_ptr<Command>> send_commands;

    bool test = false;
    std::string case_path;
};

Arguments parse_arguments(int argc, char* argv[]);

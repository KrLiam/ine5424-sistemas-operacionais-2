// process_runner.h
#pragma once

#include <string>
#include <memory>
#include "communication/reliable_communication.h"

#include "command.h"

struct Arguments {
    std::string node_id;
    std::vector<int> faults;
    std::vector<std::shared_ptr<Command>> send_commands;
};

Arguments parse_arguments(int argc, char* argv[]);

struct ThreadArgs {
    ReliableCommunication* communication{};
};

void server(ThreadArgs* args);
void client(ThreadArgs* args);
void run_process(const Arguments& args);

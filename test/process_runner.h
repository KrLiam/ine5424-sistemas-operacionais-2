// process_runner.h
#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "communication/reliable_communication.h"

#include "arguments.h"
#include "command.h"
#include "process.h"

struct CaseFile {
    std::string config_path;
    std::unordered_map<std::string, std::shared_ptr<Command>> procedures;
    bool auto_init = true;
    uint32_t min_lifespan = 0;

    static CaseFile parse_file(const std::string& path);
    static CaseFile parse(const std::string& value);
};

class Runner {
    const Arguments& args;
    
    void run_node(
        const std::string& id,
        std::shared_ptr<Command> command,
        const Config& config,
        bool execute_client,
        bool auto_init,
        uint32_t min_lifespan
    );

    void client(Process& proc);

public:
    Runner(const Arguments& args);

    void run();

    void run_test(const std::string& case_path);
};


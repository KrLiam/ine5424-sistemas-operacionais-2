#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include "communication/reliable_communication.h"

#include "arguments.h"
#include "command.h"
#include "process.h"

struct NodeProcedure {
    std::optional<SocketAddress> address;
    std::shared_ptr<Command> command;
};
struct CaseFile {
    std::string config_path;
    std::unordered_map<std::string, NodeProcedure> procedures;
    bool auto_init = true;
    uint32_t min_lifespan = 0;
    std::optional<LogLevel::Type> log_level;

    static CaseFile parse_file(const std::string& path);
    static CaseFile parse(const std::string& value);
};

class Runner {
    const Arguments& args;
    
    void run_node(
        std::string id,
        const std::optional<SocketAddress>& address,
        std::shared_ptr<Command> command,
        Config config,
        bool execute_client,
        bool auto_init,
        uint32_t min_lifespan
    );

    void client(Process& proc);

public:
    Runner(const Arguments& args);

    void run();

    void run_test(const std::string& case_path);

    void run_benchmark();
};


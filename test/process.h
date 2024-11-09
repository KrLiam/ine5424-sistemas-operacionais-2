#pragma once

#include <memory>
#include <string>

#include "communication/reliable_communication.h"
#include "command.h"

struct ThreadArgs {
    ReliableCommunication* communication{};
};

struct ExecutionContext {
    std::vector<std::unique_ptr<std::thread>> threads;

    ExecutionContext();
    ~ExecutionContext();

    void wait_complete();
};

struct Process {
    std::string node_id;
    int buffer_size;
    Config config;

    std::unique_ptr<ReliableCommunication> comm;
    ThreadArgs thread_args;
    std::thread server_receive_thread;
    std::thread server_deliver_thread;

    Process(const std::string& node_id, int buffer_size, const Config& config);

    ~Process();

    bool initialized();

    void receive(ThreadArgs* args);
    void deliver(ThreadArgs* args);

    void init();

    void kill();

    bool send_message(
        std::string node_id,
        MessageData data,
        [[maybe_unused]] std::string cmd_name,
        [[maybe_unused]] std::string data_description
    );

    void execute(const Command& command);
    void execute(std::vector<std::shared_ptr<Command>> commands);

private:

    void execute(const Command& command, ExecutionContext& ctx);
};

#pragma once

#include <memory>
#include <string>

#include "communication/reliable_communication.h"
#include "command.h"

struct ThreadArgs {
    ReliableCommunication* communication{};
};

struct Process {
    std::function<std::unique_ptr<ReliableCommunication> ()> create_comm;
    std::function<void(ThreadArgs*)> receive_routine;
    std::function<void(ThreadArgs*)> deliver_routine;

    std::unique_ptr<ReliableCommunication> comm;
    ThreadArgs thread_args;
    std::thread server_receive_thread;
    std::thread server_deliver_thread;

    Process(
        std::function<std::unique_ptr<ReliableCommunication> ()> create_comm,
        std::function<void(ThreadArgs*)> receive_routine,
        std::function<void(ThreadArgs*)> deliver_routine
    );

    ~Process();

    bool initialized();

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
};

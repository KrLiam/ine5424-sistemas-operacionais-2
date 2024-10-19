#pragma once

#include <memory>

#include "communication/reliable_communication.h"

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
};

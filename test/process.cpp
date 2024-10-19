#include "process.h"

Process::Process(
    std::function<std::unique_ptr<ReliableCommunication> ()> create_comm,
    std::function<void(ThreadArgs*)> receive_routine,
    std::function<void(ThreadArgs*)> deliver_routine
) :
    create_comm(create_comm),
    receive_routine(receive_routine),
    deliver_routine(deliver_routine)
{};

Process::~Process() {
    if (initialized()) kill();
}

bool Process::initialized() { return comm.get(); }

void Process::init() {
    if (initialized()) return;

    comm = create_comm();

    thread_args = { comm.get() };
    server_receive_thread = std::thread(receive_routine, &thread_args);
    server_deliver_thread = std::thread(deliver_routine, &thread_args);
}

void Process::kill() {
    if (!initialized()) return;

    comm.reset();

    if (server_receive_thread.joinable()) server_receive_thread.join();
    if (server_deliver_thread.joinable()) server_deliver_thread.join();
}

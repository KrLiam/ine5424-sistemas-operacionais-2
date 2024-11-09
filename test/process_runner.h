// process_runner.h
#pragma once

#include <string>
#include <memory>
#include "communication/reliable_communication.h"

#include "arguments.h"
#include "command.h"
#include "process.h"


class Runner {
    const Arguments& args;
    std::unique_ptr<Process> proc;

    void server_receive(ThreadArgs* args);
    void server_deliver(ThreadArgs* args);
    
    void client();

public:
    Runner(const Arguments& args);

    void run();
};


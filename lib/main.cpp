#include <iostream>
#include <vector>
#include <thread>

#include "core/log.h"
#include "communication/node.h"
#include "core/format.h"
#include "communication/reliable_communication.h"

std::size_t BUFFER_SIZE = 1024;

struct Arguments {
    std::string node_id;
};

Arguments parse_arguments(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::invalid_argument(
            format("Missing node id argument. Usage:\n%s <id-int>", argv[0])
        );
    }

    ConfigReader reader(argv[1]);
    std::string node_id = reader.read_word();

    if (!reader.eof()) {
        throw std::invalid_argument(
            format("Node id must be alphanumeric.", argv[1])
        );
    }

    return Arguments{node_id};
}

struct ThreadArgs {
    ReliableCommunication* comm;
};

void* server(void *args) {
    ThreadArgs* targs = (ThreadArgs*) args;
    ReliableCommunication* comm = targs->comm;

    char buffer[BUFFER_SIZE];
    std::size_t len = -1;
    while (len != 0) {
        len = comm->receive(buffer);
        log_info("Received '", std::string(buffer).c_str(), "' from <IP>.");
    }
    return NULL;
}

void* client(void *args) {
    ThreadArgs* targs = (ThreadArgs*) args;
    ReliableCommunication* comm = targs->comm;

    char msg[BUFFER_SIZE] = "Hello";
    while (true) {
        std::string id;
        std::cin >> id;

        if (id == "exit") break;

        comm->send(id, msg);
    }
    
    log_info("Exited client.");
    return NULL;
}

void run_process(std::string node_id)
{
    ReliableCommunication comm(node_id, BUFFER_SIZE);

    Node local_node = comm.get_node(node_id);
    log_info("Local node endpoint is ", local_node.get_address().to_string() , ".");

    ThreadArgs targs = { &comm };
    pthread_t server_thread;
    pthread_t client_thread;
    pthread_create(&server_thread, NULL, server, &targs);
    pthread_create(&client_thread, NULL, client, &targs);
    pthread_join(client_thread, NULL);
}

int main(int argc, char* argv[]) {
    try {
        Arguments args = parse_arguments(argc, argv);
        run_process(args.node_id);
    }
    catch (std::exception& error) {
        log_error(error.what());
    }
}

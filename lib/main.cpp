#include <iostream>
#include <vector>
#include <thread>
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"

const std::size_t BUFFER_SIZE = Packet::DATA_SIZE;

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
    ReliableCommunication* communication;
};

void server(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char buffer[BUFFER_SIZE];
    while (true) {
        Segment segment = comm->receive(buffer);
        if (segment.data_size == 0) break;
        log_info("Received '", std::string(buffer).c_str(), "' from <IP>.");
    }
}

void client(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char msg[BUFFER_SIZE] = "Hello";
    while (true) {
        std::string id;
        std::cin >> id;
        if (id == "exit") break;
        comm->send(id, msg);
    }
    log_info("Exited client.");
}

void run_process(const std::string& node_id) {
    ReliableCommunication comm(node_id, BUFFER_SIZE);
    Node local_node = comm.get_node(node_id);
    log_info("Local node endpoint is ", local_node.get_address().to_string(), ".");

    ThreadArgs targs = { &comm };

    std::thread server_thread(server, &targs);
    std::thread client_thread(client, &targs);

    client_thread.join();
    server_thread.detach();
}

int main(int argc, char* argv[]) {
    try {
        Arguments args = parse_arguments(argc, argv);
        run_process(args.node_id);
    }
    catch (const std::exception& error) {
        log_error(error.what());
    }
}
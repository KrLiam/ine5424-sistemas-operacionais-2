// process_runner.cpp
#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"

const std::size_t BUFFER_SIZE = 50000;

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

void server(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char buffer[BUFFER_SIZE];
    while (true) {
        Message msg = comm->receive(buffer);
        if (msg.length == 0) break;
        log_info("Received '", std::string(buffer).c_str(), "' from ", msg.origin.to_string(), ".");
        log_debug("Message has ", msg.length, " bytes.");
    }
}

void client(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char msg[BUFFER_SIZE] = "Hello";
    while (true) {
        std::string id;
        std::cin >> id;
        if (id == "exit") break;
        comm->send(id, {msg, 4000});
    }
    log_info("Exited client.");
}

void run_process(const std::string& node_id) {
    ReliableCommunication comm(node_id, BUFFER_SIZE);
    try {
        Node local_node = comm.get_group_registry()->get_local_node();
        log_info("Local node endpoint is ", local_node.get_address().to_string(), ".");
    } catch (const std::exception& e) {
        throw std::invalid_argument("Nodo não encontrado no registro.");
    }

    ThreadArgs targs = { &comm };

    std::thread server_thread(server, &targs);
    std::thread client_thread(client, &targs);

    client_thread.join();
    server_thread.detach();
}

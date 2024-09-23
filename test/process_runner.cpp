// process_runner.cpp
#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"
#include "utils/reader.h"

const std::size_t BUFFER_SIZE = 50000;

std::vector<int> parse_fault_list(Reader& reader) {
    std::vector<int> values;

    reader.expect('[');

    while (!reader.eof()) {
        char ch = reader.peek();

        if (ch == ']') break;

        if (ch == 'l' || ch == 'L') {
            reader.advance();
            values.push_back(INT_MAX);
        }
        else if (isdigit(ch)) {
            int value = reader.read_int();
            values.push_back(value);
        }
        else throw std::invalid_argument(
            format("Invalid character '%c' at pos %i", ch, reader.get_pos())
        );

        if (!reader.read(',')) break;
    }

    reader.expect(']');

    return values;
}

Arguments parse_arguments(int argc, char* argv[]) {
    std::vector<int> faults;
    std::vector<std::string> send_ids;

    std::string value;
    for (int i=1; i < argc; i++) {
        value += argv[i];
        value += " ";
    }

    Reader reader(value);

    std::string node_id = reader.read_word();

    if (!node_id.length()) throw std::invalid_argument(
        format("Missing node id argument. Usage:\n%s <id-int>", argv[0])
    );

    while (!reader.eof()) {
        char ch = reader.peek();
        if (!ch) break;

        std::string flag;

        if (ch == '-') {
            reader.advance();
            Override ovr = reader.override_whitespace(false);
            flag = reader.read_word();
        }
        else {
            throw std::invalid_argument(
                format("Invalid character '%c at pos %i", ch, reader.get_pos())
            );  
        }

        if (!flag.length()) throw std::invalid_argument(
            format("Missing flag name after - at pos %s", reader.get_pos())
        );

        if (flag == "f") {
            faults = parse_fault_list(reader);
        }
        else if (flag == "s") {
            std::string id = reader.read_word();
            send_ids.push_back(id);
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return Arguments{node_id, faults, send_ids};
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

        bool success = comm->send(id, {msg, 4000});

        if (success) {
            log_info("Successfuly sent message to ", id, "!");
        }
        else {
            log_error("Could not send message to ", id, ".");
        }
    }
    log_info("Exited client.");
}


struct SenderThreadArgs {
    ReliableCommunication* comm;
    std::string send_id;
};

void send_thread(SenderThreadArgs* args) {
    size_t bytes = 4000;

    log_info("Sending ", bytes, " bytes to node ", args->send_id.c_str());

    char buffer[bytes];
    bool success = args->comm->send(args->send_id, {buffer, bytes});

    if (success) {
        log_info("Sent message to ", args->send_id);
    }
    else {
        log_error("Failed to send message to ", args->send_id);
    }
}

void run_process(const Arguments& args) {
    FaultConfig fault = { faults : args.faults };
    ReliableCommunication comm(args.node_id, BUFFER_SIZE, fault);

    try {
        Node local_node = comm.get_group_registry()->get_local_node();
        log_info("Local node endpoint is ", local_node.get_address().to_string(), ".");
    } catch (const std::exception& e) {
        throw std::invalid_argument("Nodo não encontrado no registro.");
    }

    ThreadArgs targs = { &comm };

    std::thread server_thread(server, &targs);
    std::thread client_thread(client, &targs);

    int thread_num = args.send_ids.size();
    std::vector<std::unique_ptr<std::thread>> threads;
    auto thread_args = std::make_unique<SenderThreadArgs[]>(thread_num);

    for (int i=0; i < thread_num; i++) {
        thread_args[i] = {&comm, args.send_ids[i]};
        threads.push_back(std::make_unique<std::thread>(send_thread, &thread_args[i]));
    }
    
    for (std::unique_ptr<std::thread>& thread : threads) {
        thread->join();
    }

    client_thread.join();
    server_thread.detach();
}

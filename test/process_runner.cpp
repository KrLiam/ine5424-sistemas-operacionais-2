// process_runner.cpp
#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"
#include "utils/reader.h"

#include "command.h"

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



struct SenderThreadArgs {
    ReliableCommunication* comm;
    std::shared_ptr<Command> command;
};

void send_thread(SenderThreadArgs* args) {
    std::shared_ptr<Command> command = args->command;
    ReliableCommunication* comm = args->comm;

    bool success = false;

    if (command->type == CommandType::text) {
        TextCommand* cmd = static_cast<TextCommand*>(command.get());

        std::string& text = cmd->text;
        std::string& send_id = cmd->send_id;

        std::string name = (cmd->type == CommandType::text) ? "text" : "file";

        log_info("Executing command '", name, "', sending '", text, "' to node ", send_id, ".");

        MessageData data(text.c_str(), text.length());
        success = comm->send(send_id, data);
    }

    if (success) {
        log_info("Sent message.");
    }
    else {
        log_error("Failed to send message.");
    }
}

void parallelize(ReliableCommunication& comm, std::vector<std::shared_ptr<Command>>& commands) {
    int thread_num = commands.size();
    std::vector<std::unique_ptr<std::thread>> threads;
    auto thread_args = std::make_unique<SenderThreadArgs[]>(thread_num);

    for (int i=0; i < thread_num; i++) {
        thread_args[i] = {&comm, commands[i]};
        threads.push_back(std::make_unique<std::thread>(send_thread, &thread_args[i]));
    }
    
    for (std::unique_ptr<std::thread>& thread : threads) {
        thread->join();
    }
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

    while (true) {
        std::string input;
        getline(std::cin, input);

        if (input == "exit") break;

        std::vector<std::shared_ptr<Command>> commands;

        try {
            Reader reader(input);
            commands = parse_commands(reader);
        }
        catch (std::invalid_argument& err) {
            log_print(err.what());
        }
        catch (parse_error& err) {
            log_print(err.what());
        }

        parallelize(*comm, commands);
    }

    log_info("Exited client.");
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

    // parallelize(comm, args.send_ids);

    client_thread.join();
    server_thread.detach();
}

// process_runner.cpp
#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"
#include "utils/reader.h"
#include "utils/uuid.h"

#include "command.h"

const std::size_t BUFFER_SIZE = Message::MAX_MESSAGE_SIZE;

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
    std::vector<std::shared_ptr<Command>> send_commands;

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
            send_commands = parse_commands(reader);
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return Arguments{node_id, faults, send_commands};
}


void create_dummy_data(char* data, size_t size) {
    int num = 0;
    size_t pos = 0;
    
    while (pos < size) {
        if (pos > 0) {
            data[pos] = ' ';
            pos++;
        }

        if (pos >= size) break;

        std::string value = std::to_string(num);
        num++;

        for (char ch : value) {
            data[pos] = ch;
            pos++;

            if (pos >= size) break;
        }
    }
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
        std::string name = cmd->name();

        log_info("Executing command '", name, "', sending '", text, "' to node ", send_id, ".");

        success = comm->send(send_id, {text.c_str(), text.length()});
    }
    else if (command->type == CommandType::dummy) {
        DummyCommand* cmd = static_cast<DummyCommand*>(command.get());

        size_t size = cmd->size;
        std::string& send_id = cmd->send_id;
        std::string name = cmd->name();

        std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
        create_dummy_data(data.get(), size);

        log_info("Executing command '", name, "', sending ", size, " bytes of dummy data to node ", send_id, ".");

        success = comm->send(send_id, {data.get(), size});
    }
    else if (command->type == CommandType::file) {
        FileCommand* cmd = static_cast<FileCommand*>(command.get());

        std::string& path = cmd->path;
        std::string& send_id = cmd->send_id;
        std::string name = cmd->name();

        std::ifstream file(path, std::ios::binary | std::ios::ate);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        char buffer[size];
        if (file.read(buffer, size))
        {
            log_info("Executing command '", name, "', sending ", size, " bytes of dummy data to node ", send_id, ".");

            success = comm->send(send_id, {buffer, size});
        }
    }

    if (success) {
        log_info("Sent message.");
    }
    else {
        log_error("Failed to send message.");
    }
}

void parallelize(ReliableCommunication& comm, const std::vector<std::shared_ptr<Command>>& commands) {
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
        ReceiveResult result = comm->receive(buffer);
        if (result.bytes == 0) break;
        log_print("Received '", std::string(buffer, result.bytes).c_str(), "' (", result.bytes, " bytes) from ", result.sender_id);

        std::string output_filename = UUID().as_string();
        std::ofstream file(output_filename);
        file.write(buffer, result.bytes);
        log_print("Saved message to file [", output_filename, "].");

    }
}

void client(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;

    while (true) {
        std::string input;

        std::cout << "> ";
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

    parallelize(comm, args.send_commands);

    client_thread.join();
    server_thread.detach();
}

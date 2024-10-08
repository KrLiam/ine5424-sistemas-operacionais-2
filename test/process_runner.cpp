// process_runner.cpp

#include <sys/stat.h>

#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"
#include "utils/reader.h"
#include "utils/uuid.h"

#include "command.h"

const std::size_t BUFFER_SIZE = Message::MAX_SIZE;

std::string get_available_flags(const char* program_name) {
    std::string result;
    result += BOLD_GREEN "Usage: " COLOR_RESET;
    result += program_name;
    result += " <node_id> [flags]\n";

    result += BOLD_CYAN "Available commands:\n" COLOR_RESET;
    result += YELLOW "  text " H_BLACK "<" WHITE "message" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a message to the node with id <id>.\n";
    result += YELLOW "  file " H_BLACK "<" WHITE "path" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a file to the node with id <id>.\n";
    result += YELLOW "  dummy " H_BLACK "<" WHITE "size" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a dummy message of size <size> to the node with id <id>.\n";
    result += YELLOW "  help: " COLOR_RESET "Show the help message.\n";
    result += YELLOW "  exit: " COLOR_RESET "Terminates the process.\n";

    result += BOLD_CYAN "\nAvailable flags:\n" COLOR_RESET;
    result += YELLOW "  -s " H_BLACK "'" WHITE "<commands>" H_BLACK "'" COLOR_RESET ": Executes commands at process start.\n";
    result += YELLOW "  -f " H_BLACK "<" WHITE "fault-list" H_BLACK ">" COLOR_RESET ": Defines faults for packet reception based on a fault list.\n";

    return result;
}

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


bool send_message(
    ReliableCommunication* comm,
    std::string node_id,
    MessageData data,
    std::string cmd_name,
    std::string data_description
) {
    if (node_id == BROADCAST_ID) {
        log_info(
            "Executing command '", cmd_name, "', broadcasting ", data_description, "."
        );
        return comm->broadcast(data);
    }

    log_info(
        "Executing command '", cmd_name, "', sending ", data_description, " to node ", node_id, "."
    );
    return comm->send(node_id, data);
}
void send_thread(SenderThreadArgs* args) {
    std::shared_ptr<Command> command = args->command;
    ReliableCommunication* comm = args->comm;

    try {
        bool success = false;
        std::string send_id;

        if (command->type == CommandType::text) {
            TextCommand* cmd = static_cast<TextCommand*>(command.get());

            std::string& text = cmd->text;

            success = send_message(
                comm,
                cmd->send_id,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
        }
        else if (command->type == CommandType::broadcast) {
            BroadcastCommand* cmd = static_cast<BroadcastCommand*>(command.get());

            std::string& text = cmd->text;

            success = send_message(
                comm,
                BROADCAST_ID,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
        }
        else if (command->type == CommandType::dummy) {
            DummyCommand* cmd = static_cast<DummyCommand*>(command.get());

            size_t size = cmd->size;

            std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
            create_dummy_data(data.get(), size);

            success = send_message(
                comm,
                cmd->send_id,
                {data.get(), size},
                cmd->name(),
                format("%u bytes of dummy data", size)
            );
        }
        else if (command->type == CommandType::file) {
            FileCommand* cmd = static_cast<FileCommand*>(command.get());

            std::string& path = cmd->path;

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            char buffer[size];
            if (file.read(buffer, size))
            {
                success = send_message(
                    comm,
                    cmd->send_id,
                    {buffer, size},
                    cmd->name(),
                    format("%u bytes from file '%s'", size, path.c_str())
                );
            }
        }

        bool broadcast = send_id == BROADCAST_ID;

        if (success && !broadcast) {
            log_print("Successfuly sent message to node ", send_id, ".");
        }
        else if (success && broadcast) {
            log_print("Successfuly broadcasted message.");
        }
        else if (!success && broadcast) {
            log_print("Could not broadcast message.");
        }
        else {
            log_error("Could not send message to node ", send_id, ".");
        }
    }
    catch (std::invalid_argument& err) {
        log_error(err.what());
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
        ReceiveResult result;
        try {
            result = comm->receive(buffer);
        }
        catch (buffer_termination& err) {
            return;
        }
        
        if (result.length == 0) break;

        std::string message_data(buffer, result.length);

        if (message_data.length() > 100 )
        {
            message_data = message_data.substr(0, 100) + "...";
        }

        log_print("Received '", message_data.c_str(), "' (", result.length, " bytes) from ", result.sender_id);

        mkdir("messages", S_IRWXU);
        std::string output_filename = "messages/" + UUID().as_string();
        std::ofstream file(output_filename);
        file.write(buffer, result.length);
        log_print("Saved message to file [", output_filename, "].");

    }
}

void client(ReliableCommunication& comm) {
    while (true) {
        std::string input;

        std::cout << "> ";
        getline(std::cin, input);

        if (input == "exit") break;
        if (input == "help") {
            std::cout << get_available_flags("program") << std::endl;
            continue;
        }

        std::vector<std::shared_ptr<Command>> commands;

        try {
            Reader reader(input);
            commands = parse_commands(reader);
        }
        catch (std::invalid_argument& err) {
            std::cout << "Unknown command '" << input << "'.\n--help to see the list of commands." << std::endl;
            continue;
        }
        catch (parse_error& err) {
            log_print(err.what());
        }

        parallelize(comm, commands);
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

    parallelize(comm, args.send_commands);

    client(comm);

    comm.shutdown();
    server_thread.join();
}

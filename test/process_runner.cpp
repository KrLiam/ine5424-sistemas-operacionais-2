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


std::string get_available_flags(const char* program_name) {
    std::string result;
    result += BOLD_GREEN "Usage: " COLOR_RESET;
    result += program_name;
    result += " <node_id> [flags]\n";

    result += BOLD_CYAN "Available commands:\n" COLOR_RESET;
    result += YELLOW "  text " H_BLACK "<" WHITE "message" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a message to the node with id <id>.\n";
    result += YELLOW "  file " H_BLACK "<" WHITE "path" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a file to the node with id <id>.\n";
    result += YELLOW "  dummy " H_BLACK "<" WHITE "size" H_BLACK "> <" WHITE "count" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send <count> dummy messages of size <size> to the node with id <id>.\n";
    result += YELLOW "  kill: " COLOR_RESET "Kills the local node.\n";
    result += YELLOW "  init: " COLOR_RESET "Initializes the local node.\n";
    result += YELLOW "  help: " COLOR_RESET "Show the help message.\n";
    result += YELLOW "  exit: " COLOR_RESET "Exits session.\n";

    result += BOLD_CYAN "\nAvailable flags:\n" COLOR_RESET;
    result += YELLOW "  -s " H_BLACK "'" WHITE "<commands>" H_BLACK "'" COLOR_RESET ": Executes commands at process start.\n";
    result += YELLOW "  -f " H_BLACK "<" WHITE "fault-list" H_BLACK ">" COLOR_RESET ": Defines faults for packet reception based on a fault list.\n";

    return result;
}


Runner::Runner(const Arguments& args) : args(args) {
    proc = std::make_unique<Process>(
        [&args]() {
            return std::make_unique<ReliableCommunication>(args.node_id, Message::MAX_SIZE);
        }
    );
}

void Runner::client() {
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
            std::cout << "Unknown command '" << input << "'.\nhelp to see the list of commands." << std::endl;
            continue;
        }
        catch (parse_error& err) {
            log_print(err.what());
        }

        proc->execute(commands);
    }

    log_info("Exited client.");
}

void Runner::run() {
    proc->init();

    try {
        Node local_node = proc->comm->get_group_registry()->get_local_node();
        log_info("Local node endpoint is ", local_node.get_address().to_string(), ".");
    } catch (const std::exception& e) {
        throw std::invalid_argument("Nodo não encontrado no registro.");
    }

    proc->execute(args.send_commands);

    client();
}
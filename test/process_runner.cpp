// process_runner.cpp
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

#include "process_runner.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/constants.h"
#include "utils/format.h"
#include "communication/reliable_communication.h"
#include "utils/reader.h"
#include "utils/uuid.h"

#include "command.h"

namespace fs = std::filesystem;

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


CaseFile CaseFile::parse_file(const std::string& path) {
    return  CaseFile::parse(read_file(path));
}
CaseFile CaseFile::parse(const std::string& value) {
    Reader reader(value);
    CaseFile f;

    while (reader.peek()) {
        int pos = reader.get_pos();
        std::string target = reader.read_word();

        if (!target.length()) {
            throw parse_error(
                format("Unexpected '%c' at pos %i of config file.", reader.peek(), reader.get_pos())
            );
        }

        reader.expect('=');

        if (target == "config") {
            f.config_path = parse_path(reader);
        }
        else if (target == "min_lifespan") {
            f.min_lifespan = reader.read_int();
        }
        else if (target == "auto_init") {
            f.auto_init = reader.read_int();
        }
        else if (target == "procedures") {
            reader.expect('{');

            while (!reader.eof()) {
                char p = reader.peek();
                if (!p || p == '}') break;

                std::string key = reader.read_word();

                if (!key.length()) throw parse_error(
                    format("Unexpected '%c' at pos %i of procedures in case file.", p, reader.get_pos())
                );
                if (f.procedures.contains(key)) throw parse_error(
                    format("Key '%s' was defined twice at procedures.", key.c_str())
                );

                reader.expect('=');
                std::shared_ptr<Command> value = parse_command_list(reader);

                f.procedures.emplace(key, value);

                if (!reader.read(',')) break;
            }

            reader.expect('}');
        }
        else throw parse_error(format("Invalid '%s' at pos %i", target.c_str(), pos));
    }

    return f;
}


Runner::Runner(const Arguments& args) : args(args) {}

void Runner::client(Process& proc) {
    Logger::set_prefix("\r");

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

        proc.execute(commands);
    }

    log_info("Exited client.");
}

void Runner::run() {
    if (args.test) {
        run_test(args.case_path);
        return;
    }

    Config config = ConfigReader::parse_file(DEFAULT_CONFIG_PATH);
    auto commands = std::make_shared<SequenceCommand>(args.send_commands);
    run_node(args.node_id, commands, config, true, true, 0);
}


void tail_dir(const char* dir) {
    std::string command = format("clear -x && /usr/bin/tail -n 5 %s/*", dir);

    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) return;

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        std::cout << buffer.data();
    }
}

void Runner::run_test(const std::string& case_path_str) {
    fs::path case_path = case_path_str;

    log_info("Parsing case file in ", case_path);
    CaseFile f = CaseFile::parse_file(case_path);

    fs::path config_path = fs::absolute(
        case_path.parent_path() / f.config_path
    ).lexically_normal().lexically_relative(fs::current_path());

    log_info("Parsing config file in ", config_path);
    Config config = ConfigReader::parse_file(config_path); // TODO fazer resolução do caminho a partir do .case

    log_info("Initializing test case...");

    std::string case_name = fs::path(case_path).stem().string();
    fs::path case_dir_path = format(DATA_DIR "/cases/%s", case_name.c_str());
    mkdir(DATA_DIR, S_IRWXU);
    mkdir(DATA_DIR "/cases", S_IRWXU);
    mkdir(case_dir_path.c_str(), S_IRWXU);

    std::vector<std::pair<std::string, pid_t>> pids;

    for (const auto& [id, command] : f.procedures) {
        pid_t pid = fork();

        if (pid < 0) throw std::runtime_error("Could not fork process.");

        if (pid == 0) {
            Logger::set_colored(false);
            Logger::show_files(false);
            Logger::set_output_file(format("%s/%s.log", case_dir_path.c_str(), id.c_str()));

            run_node(id, command, config, false, f.auto_init, f.min_lifespan);
            return;
        }

        pids.push_back({id, pid});

        log_info("Created node ", id, " (pid=", pid, ").");
    }

    pid_t tail_pid = fork();
    if (tail_pid == 0) {
        while (true) {
            tail_dir(case_dir_path.c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    for (auto [id, pid] : pids) {
        int status;
        waitpid(pid, &status, 0);
    }

    if (tail_pid > 0) kill(tail_pid, 15);

    tail_dir(case_dir_path.c_str());

    log_print("");
    log_info("Test completed.");
}

void Runner::run_node(
    const std::string& id,
    std::shared_ptr<Command> command,
    const Config& config,
    bool execute_client,
    bool auto_init,
    uint32_t min_lifespan
) {
    uint64_t start_time = DateUtils::now();
    Process proc(id, Message::MAX_SIZE, config);

    if (auto_init) {
        proc.init();
    }

    proc.execute(*command);

    if (execute_client) {
        client(proc);
    }

    // forçar o nó a permanecer vivo por um tempo mínimo determinado
    uint64_t end_time = DateUtils::now();
    uint64_t lifespan = end_time - start_time;
    if (lifespan < min_lifespan) {
        std::this_thread::sleep_for(std::chrono::milliseconds(min_lifespan - lifespan));
    }
}
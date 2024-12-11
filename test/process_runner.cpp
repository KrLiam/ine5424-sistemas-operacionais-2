#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <filesystem>

#include "process_runner.h"
#include "benchmarker.h"
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
    result += YELLOW "  text " H_BLACK "<" WHITE "message" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a message to the node with id <id>. The keyword 'text' is optional.\n";
    result += YELLOW "  file " H_BLACK "<" WHITE "path" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a file to the node with id <id>.\n";
    result += YELLOW "  dummy " H_BLACK "<" WHITE "size" H_BLACK ">" COLOR_RESET " -> " H_BLACK "<" WHITE "id" H_BLACK ">" COLOR_RESET ": Send a dummy message of <size> bytes to the node with id <id>.\n";
    result += YELLOW "  kill: " COLOR_RESET "Kills the local node.\n";
    result += YELLOW "  init: " COLOR_RESET "Initializes the local node if not already active.\n";
    result += YELLOW "  sleep " H_BLACK "<" WHITE "ms" H_BLACK ">" COLOR_RESET ": Makes the node sleep for <ms> milliseconds.\n";
    result += YELLOW "  repeat " H_BLACK "<" WHITE "n" H_BLACK "> <" WHITE "subcommand" H_BLACK ">" COLOR_RESET ": Repeats a subcommand <n> times.\n";
    result += YELLOW "  [<subcommand_1>, ..., <subcommand_n>]: " COLOR_RESET "Executes commands sequentially.\n";
    result += YELLOW "  async <subcommand>: " COLOR_RESET "Executes a subcommand in parallel.\n";
    result += YELLOW "  fault {drop <number>/<fragment>[u|b|a] [<n>% | <n>x],}: " COLOR_RESET "Injects failures in the local node.\n";
    result += YELLOW "  exit: " COLOR_RESET "Exits the process.\n";
    result += YELLOW "  help: " COLOR_RESET "Displays this help message.\n";

    result += BOLD_CYAN "\nAvailable flags:\n" COLOR_RESET;
    result += YELLOW "  -s " H_BLACK "'" WHITE "<commands>" H_BLACK "'" COLOR_RESET ": Executes commands at process start.\n";

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
        else if (target == "log_level") {
            f.log_level = LogLevel::parse(reader);
        }
        else if (target == "procedures") {
            reader.expect('{');

            while (!reader.eof()) {
                char p = reader.peek();
                if (!p || p == '}') break;

                std::string key = reader.read_word();

                std::optional<SocketAddress> address = std::nullopt;
                if (reader.read('@')) {
                    address = SocketAddress::parse(reader);
                }
                if (!key.length() && address.has_value()) key = generate_node_id(*address);

                if (!key.length()) throw parse_error(
                    format("Unexpected '%c' at pos %i of procedures in case file.", p, reader.get_pos())
                );
                if (f.procedures.contains(key)) throw parse_error(
                    format("Key '%s' was defined twice at procedures.", key.c_str())
                );

                reader.expect('=');
                std::shared_ptr<Command> value = parse_command_list(reader);

                NodeProcedure procedure{address, value};
                f.procedures.emplace(key, procedure);

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
    if (args.log_level.has_value()) Logger::set_level(*args.log_level);

    if (args.test) {
        run_test(args.case_path);
        return;
    }

    if (args.benchmark) {
        run_benchmark();
        return;
    }

    Config config = ConfigReader::parse_file(DEFAULT_CONFIG_PATH);
    auto commands = std::make_shared<SequenceCommand>(args.send_commands);
    run_node(args.node_id, args.address, commands, config, true, true, 0);
}


void tail_dir(const std::string& log_directory, const std::string& case_file_path) {
    std::string command = format("tail -n 5 %s/*", log_directory.c_str());

    std::array<char, 128> buffer;
    std::string result;

    auto pclose_f = [](FILE* fp) { pclose(fp); };
    std::unique_ptr<FILE, decltype(pclose_f)> pipe(popen(command.c_str(), "r"), pclose_f);
    if (!pipe) return;

    std::ostringstream oss;

    oss << CLEAR_SCREEN << "Case file: " << case_file_path << std::endl << std::endl;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        oss << buffer.data();
    }
    std::cout << oss.str() << std::flush;
}

pid_t create_tail_process(std::string log_directory, std::string case_file_path) {
    pid_t pid = fork();
    if (pid != 0) return pid;

    tail_dir(log_directory, case_file_path);

    int inotifyFd = inotify_init();
    if (inotifyFd < 0) exit(EXIT_FAILURE);

    int watchDescriptor = inotify_add_watch(inotifyFd, log_directory.c_str(), IN_MODIFY);
    if (watchDescriptor < 0) {
        close(inotifyFd);
        exit(EXIT_FAILURE);
    }

    const size_t bufferSize = 1024 * (sizeof(struct inotify_event) + 16);
    char buffer[bufferSize];

    while (true) {
        int length = read(inotifyFd, buffer, bufferSize);
        if (length < 0) break;

        for (int i = 0; i < length;) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];

            if ((event->mask & IN_MODIFY) && (event->len > 0)) {
                std::string filename = event->name;
                if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".log") {
                    tail_dir(log_directory, case_file_path);
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    close(inotifyFd);
    exit(EXIT_SUCCESS);
}

void Runner::run_test(const std::string& case_path_str) {
    fs::path case_path = case_path_str;

    log_info("Parsing case file in ", case_path);
    CaseFile f = CaseFile::parse_file(case_path);

    fs::path config_path = fs::absolute(
        case_path.parent_path() / f.config_path
    ).lexically_normal().lexically_relative(fs::current_path());

    log_info("Parsing config file in ", config_path);
    Config config = ConfigReader::parse_file(config_path);

    log_info("Initializing test case...");

    std::string case_name = fs::path(case_path).stem().string();
    fs::path case_dir_path = format(DATA_DIR "/cases/%s", case_name.c_str());
    mkdir(DATA_DIR, S_IRWXU);
    mkdir(DATA_DIR "/cases", S_IRWXU);
    mkdir(case_dir_path.c_str(), S_IRWXU);

    std::vector<std::pair<std::string, pid_t>> pids;

    for (const auto& [id, proc] : f.procedures) {
        pid_t pid = fork();

        if (pid < 0) throw std::runtime_error("Could not fork process.");

        if (pid == 0) {
            Logger::set_colored(false);
            Logger::show_files(false);
            Logger::set_output_file(format("%s/%s.log", case_dir_path.c_str(), id.c_str()));
            if (f.log_level) Logger::set_level(*f.log_level);


            run_node(id, proc.address, proc.command, config, false, f.auto_init, f.min_lifespan);
            return;
        }

        pids.push_back({id, pid});

        log_info("Created node ", id, " (pid=", pid, ").");
    }

    bool log_tail = args.log_tail;
    if (pids.size() > 5 && args.log_tail && !args.specified_log_tail) {
        log_tail = false;
        log_warn(
            "Log tail is disabled by default for large groups, "
            "use ./program case ... -log-tail 1 or make test case=... log_tail=1 "
            "to explicitly enable it."
        );
    }

    pid_t tail_pid;
    if (log_tail) tail_pid = create_tail_process(case_dir_path, case_path);

    for (auto [id, pid] : pids) {
        int status;
        waitpid(pid, &status, 0);
    }

    if (log_tail) {
        if (tail_pid > 0) kill(tail_pid, 15);
        tail_dir(case_dir_path, case_path);
    }

    log_print("");
    log_info("Test completed.");
}

void Runner::run_node(
    std::string id,
    const std::optional<SocketAddress>& opt_address,
    std::shared_ptr<Command> command,
    Config config,
    bool execute_client,
    bool auto_init,
    uint32_t min_lifespan
) {
    uint64_t start_time = DateUtils::now();

    if (!config.get_node(id)) {
        if (opt_address.has_value()) {
            const SocketAddress& address = *opt_address;

            if (const NodeConfig* node = config.get_node(*args.address)) {
                id = node->id;
            }
            else {
                if (!id.length()) id = generate_node_id(address);
                config.nodes.push_back(NodeConfig{id, address});
            }
        }
        else {
            if (id.length()) throw std::invalid_argument(
                format(
                    "Node '%s' is not default, specify the address to be used.\nUsage: %s %s -a <address>",
                    id.c_str(),
                    args.program_name.c_str(),
                    id.c_str()
                )
            );
            throw std::invalid_argument(format(
                "Missing node arguments. Either provide a node id that is present in the config file or provide a valid port."
                "\nUsage: %s <config-node-id>"
                "\nUsage: %s <id> -a <address>"
                "\nUsage: %s -a <address>",
                args.program_name.c_str(),
                args.program_name.c_str(),
                args.program_name.c_str()
            ));
        }
    }

    Process proc(id, Message::MAX_SIZE, !args.test, args.verbose, config);

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
        log_info("Node lifespan was ", lifespan, ", waiting for ", min_lifespan - lifespan);
        std::this_thread::sleep_for(std::chrono::milliseconds(min_lifespan - lifespan));
    }
}

void Runner::run_benchmark() {
    if (!args.log_level.has_value()) Logger::set_level(LogLevel::NONE);

    Benchmarker benchmarker(
        args.num_groups,
        args.num_nodes,
        args.bytes_per_node,
        args.node_interval,
        args.max_message_size,
        args.mode,
        args.no_encryption,
        args.no_checksum,
        args.max_read_operations,
        args.max_write_operations,
        args.alive,
        args.max_inactivity_time,
        args.measure_failures
    );

    BenchmarkResult result = benchmarker.run();
    if (args.out_file.has_value()) {
        std::optional<std::string> path = result.save(*args.out_file);
        if (path.has_value()) std::cout << "Result saved in '" << *path << "'." << std::endl;
    }

    if (!result.completed) {
        exit(0);
    }
}
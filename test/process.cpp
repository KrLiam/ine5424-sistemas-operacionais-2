#include <sys/stat.h>
#include <thread>
#include <chrono>

#include "process.h"

Process::Process(const std::string& node_id, int buffer_size, const Config& config)
    : node_id(node_id), buffer_size(buffer_size), config(config) {};

Process::~Process() {
    if (initialized()) kill();
}

bool Process::initialized() { return comm.get(); }

void Process::init() {
    if (initialized()) return;

    comm = std::make_unique<ReliableCommunication>(node_id, buffer_size, config);

    thread_args = { comm.get() };
    server_receive_thread = std::thread(std::bind(&Process::receive, this, _1), &thread_args);
    server_deliver_thread = std::thread(std::bind(&Process::deliver, this, _1), &thread_args);
}

void Process::kill() {
    if (!initialized()) return;

    comm.reset();

    if (server_receive_thread.joinable()) server_receive_thread.join();
    if (server_deliver_thread.joinable()) server_deliver_thread.join();
}

void create_dummy_data(char* data, size_t count, size_t size) {
    int num = 0;
    size_t pos = 0;

    std::string count_as_string = format("[%d] ", count);
    for (char ch : count_as_string) {
        if (pos >= size) break;

        data[pos] = ch;
        pos++;
    }
    
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
    Process* proc;
    std::shared_ptr<Command> command;
};


ExecutionContext::ExecutionContext() {}

ExecutionContext::~ExecutionContext() {
    wait_complete();
}

void ExecutionContext::wait_complete() {
    for (std::unique_ptr<std::thread>& thread : threads) {
        if (thread->joinable()) thread->join();
    }
}


void Process::receive(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char buffer[buffer_size];
    while (true) {
        ReceiveResult result;
        try {
            result = comm->receive(buffer);
        }
        catch (buffer_termination& err) {
            break;
        }
        
        if (result.length == 0) break;

        std::string message_data(buffer, result.length);

        if (message_data.length() > 100 )
        {
            message_data = message_data.substr(0, 100) + "...";
        }

        mkdir(DATA_DIR, S_IRWXU);
        mkdir(DATA_DIR "/messages", S_IRWXU);
        std::string output_filename = DATA_DIR "/messages/" + UUID().as_string();

        std::ofstream file(output_filename);
        file.write(buffer, result.length);
        log_print(
            "Received '",
            message_data.c_str(),
            "' (", result.length, " bytes) from ",
            result.sender_id,
            ".\nSaved message to file [", output_filename, "].");
    }

    log_info("Closed application receiver thread.");
}

void Process::deliver(ThreadArgs* args) {
    ReliableCommunication* comm = args->communication;
    char buffer[buffer_size];
    while (true) {
        ReceiveResult result;
        try {
            result = comm->deliver(buffer);
        }
        catch (buffer_termination& err) {
            break;
        }
        
        if (result.length == 0) break;

        std::string message_data(buffer, result.length);

        if (message_data.length() > 100 )
        {
            message_data = message_data.substr(0, 100) + "...";
        }

        log_print("[Broadcast] Received '", message_data.c_str(), "' (", result.length, " bytes) from ", result.sender_id);

        mkdir(DATA_DIR, S_IRWXU);
        mkdir(DATA_DIR "/messages", S_IRWXU);
        std::string output_filename = DATA_DIR "/messages/" + UUID().as_string();
        std::ofstream file(output_filename);
        file.write(buffer, result.length);
        log_print("Saved message to file [", output_filename, "].");
    }

    log_info("Closed application deliver thread.");
}

bool Process::send_message(
    std::string node_id,
    MessageData data,
    [[maybe_unused]] std::string cmd_name,
    [[maybe_unused]] std::string data_description
) {
    if (!initialized()) {
        log_print("Could not execute '", cmd_name, "', node is dead.");
        return false;
    }

    bool success = false;
    if (node_id == BROADCAST_ID) {
        log_info(
            "Executing command '", cmd_name, "', broadcasting ", data_description, "."
        );
        success = comm->broadcast(data);
    }
    else {
        log_info(
            "Executing command '", cmd_name, "', sending ", data_description, " to node ", node_id, "."
        );
        success = comm->send(node_id, data);
    }

    bool broadcast = node_id == BROADCAST_ID;

    if (success && !broadcast) {
        log_print("Successfully sent message to node ", node_id, ".");
    }
    else if (success && broadcast) {
        log_print("Successfully broadcasted message.");
    }
    else if (!success && broadcast) {
        log_print("Could not broadcast message.");
    }
    else {
        log_error("Could not send message to node ", node_id, ".");
    }

    return success;
}

// void parallelize(Process& proc, const std::vector<std::shared_ptr<Command>>& commands) {
//     int thread_num = commands.size();
//
//     if (thread_num == 1) {
//         SenderThreadArgs args = {&proc, commands[0]};
//         send_thread(&args);
//         return;
//     }
//
//     std::vector<std::unique_ptr<std::thread>> threads;
//     auto thread_args = std::make_unique<SenderThreadArgs[]>(thread_num);
//
//     for (int i=0; i < thread_num; i++) {
//         thread_args[i] = {&proc, commands[i]};
//         threads.push_back(std::make_unique<std::thread>(send_thread, &thread_args[i]));
//     }
//
//     for (std::unique_ptr<std::thread>& thread : threads) {
//         thread->join();
//     }
// }

void Process::execute(const Command& command) {
    ExecutionContext ctx;

    execute(command, ctx);

    ctx.wait_complete();
}
void Process::execute(const Command& command, ExecutionContext& ctx) {
    try {
        if (command.type == CommandType::sequence) {
            const SequenceCommand* cmd = static_cast<const SequenceCommand*>(&command);

            ExecutionContext sub_ctx;

            for (const std::shared_ptr<Command>& sub_cmd : cmd->subcommands) {
                execute(*sub_cmd, sub_ctx);
            }

            sub_ctx.wait_complete();

            return;
        }

        if (command.type == CommandType::async) {
            const AsyncCommand* cmd = static_cast<const AsyncCommand*>(&command);
            const std::shared_ptr<Command> subcommand = cmd->subcommand;

            ctx.threads.push_back(
                std::make_unique<std::thread>([this, subcommand]() {
                    execute(*subcommand);
                })
            );

            return;
        }

        if (command.type == CommandType::sleep) {
            const SleepCommand* cmd = static_cast<const SleepCommand*>(&command);

            std::this_thread::sleep_for(std::chrono::milliseconds(cmd->interval));

            return;
        }

        if (command.type == CommandType::repeat) {
            const RepeatCommand* cmd = static_cast<const RepeatCommand*>(&command);

            for (int i = 0; i < cmd->count; i++) {
                execute(*cmd->subcommand, ctx);
            }

            return;
        }

        if (command.type == CommandType::init) {
            if (initialized()) {
                log_print("Node is already initialized.");
                return;
            }
            log_print("Initializing node.");
            init();
            return;
        }

        if (!initialized()) {
            log_print("Node is dead.");
            return;
        }

        if (command.type == CommandType::text) {
            const TextCommand* cmd = static_cast<const TextCommand*>(&command);

            const std::string& text = cmd->text;

            send_message(
                cmd->send_id,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
            return;
        }
        
        if (command.type == CommandType::broadcast) {
            const BroadcastCommand* cmd = static_cast<const BroadcastCommand*>(&command);

            const std::string& text = cmd->text;

            send_message(
                BROADCAST_ID,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
            return;
        }

        if (command.type == CommandType::dummy) {
            const DummyCommand* cmd = static_cast<const DummyCommand*>(&command);

            size_t size = cmd->size;
            size_t count = cmd->count;

            for (size_t i = 0; i < count; i++)
            {
                char data[size];
                create_dummy_data(data, i, size);

                send_message(
                    cmd->send_id,
                    {data, size},
                    cmd->name(),
                    format("%u bytes of dummy data", size)
                );
            }
            return;
        }

        if (command.type == CommandType::file) {
            const FileCommand* cmd = static_cast<const FileCommand*>(&command);

            const std::string& path = cmd->path;

            std::ifstream file(path, std::ios::binary | std::ios::ate);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            char buffer[size];
            if (file.read(buffer, size))
            {
                send_message(
                    cmd->send_id,
                    {buffer, size},
                    cmd->name(),
                    format("%u bytes from file '%s'", size, path.c_str())
                );
            }
            return;
        }

        if (command.type == CommandType::kill) {
            log_print("Killing node.");
            kill();
            return;
        }

        if (command.type == CommandType::fault) {
            const FaultCommand* cmd = static_cast<const FaultCommand*>(&command);
            for (const FaultRule& rule : cmd->rules) {
                comm->add_fault_rule(rule);
                
                if (auto r = std::get_if<DropFaultRule>(&rule)) {
                    log_print("Drop ", r->to_string());
                }
            }
            return;
        }
    }
    catch (std::invalid_argument& err) {
        log_error(err.what());
    }
}

void Process::execute(std::vector<std::shared_ptr<Command>> commands) {
    execute(SequenceCommand(commands));
}

#include "process.h"

Process::Process(
    std::function<std::unique_ptr<ReliableCommunication> ()> create_comm,
    std::function<void(ThreadArgs*)> receive_routine,
    std::function<void(ThreadArgs*)> deliver_routine
) :
    create_comm(create_comm),
    receive_routine(receive_routine),
    deliver_routine(deliver_routine)
{};

Process::~Process() {
    if (initialized()) kill();
}

bool Process::initialized() { return comm.get(); }

void Process::init() {
    if (initialized()) return;

    comm = create_comm();

    thread_args = { comm.get() };
    server_receive_thread = std::thread(receive_routine, &thread_args);
    server_deliver_thread = std::thread(deliver_routine, &thread_args);
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

//     if (thread_num == 1) {
//         SenderThreadArgs args = {&proc, commands[0]};
//         send_thread(&args);
//         return;
//     }

//     std::vector<std::unique_ptr<std::thread>> threads;
//     auto thread_args = std::make_unique<SenderThreadArgs[]>(thread_num);

//     for (int i=0; i < thread_num; i++) {
//         thread_args[i] = {&proc, commands[i]};
//         threads.push_back(std::make_unique<std::thread>(send_thread, &thread_args[i]));
//     }
    
//     for (std::unique_ptr<std::thread>& thread : threads) {
//         thread->join();
//     }
// }

void Process::execute(const Command& command) {
    try {
        std::string send_id;

        if (command.type == CommandType::text) {
            const TextCommand* cmd = static_cast<const TextCommand*>(&command);

            const std::string& text = cmd->text;
            send_id = cmd->send_id;

            send_message(
                cmd->send_id,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
        }
        else if (command.type == CommandType::broadcast) {
            const BroadcastCommand* cmd = static_cast<const BroadcastCommand*>(&command);

            const std::string& text = cmd->text;

            send_message(
                BROADCAST_ID,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
        }
        else if (command.type == CommandType::dummy) {
            const DummyCommand* cmd = static_cast<const DummyCommand*>(&command);

            size_t size = cmd->size;
            size_t count = cmd->count;
            send_id = cmd->send_id;

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
        }
        else if (command.type == CommandType::file) {
            const FileCommand* cmd = static_cast<const FileCommand*>(&command);

            const std::string& path = cmd->path;
            send_id = cmd->send_id;

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
        }
        else if (command.type == CommandType::kill) {
            if (!initialized()) {
                log_print("Node is already dead.");
                return;
            }
            log_print("Killing node.");
            kill();
        }
        else if (command.type == CommandType::init) {
            if (initialized()) {
                log_print("Node is already initialized.");
                return;
            }
            log_print("Initializing node.");
            init();
        }
        else if (command.type == CommandType::fault) {
            if (!initialized()) {
                log_print("Node is dead.");
                return;
            }

            const FaultCommand* cmd = static_cast<const FaultCommand*>(&command);
            for (const FaultRule& rule : cmd->rules) {
                comm->add_fault_rule(rule);
                
                if (auto r = std::get_if<DropFaultRule>(&rule)) {
                    log_print("Drop ", r->to_string());
                }
            }
        }
    }
    catch (std::invalid_argument& err) {
        log_error(err.what());
    }
}

void Process::execute(std::vector<std::shared_ptr<Command>> commands) {
    for (std::shared_ptr<Command> command : commands) {
        execute(*command);
    }
}

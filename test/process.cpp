#include <sys/stat.h>
#include <thread>
#include <chrono>

#include "process.h"

Process::Process(
    const std::string& node_id,
    int buffer_size,
    bool save_message_files,
    bool verbose,
    const Config& config
) :
    node_id(node_id),
    buffer_size(buffer_size),
    save_message_files(save_message_files),
    verbose(verbose),
    config(config) {};

Process::~Process() {
    if (initialized()) kill();
}

bool Process::initialized() { return comm.get(); }

void Process::init() {
    if (initialized()) return;

    comm = std::make_unique<ReliableCommunication>(node_id, buffer_size, verbose, config);

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

        log_print(
            "Received '",
            message_data.c_str(),
            "' (", result.length, " bytes) from ",
            result.sender_id,
            "."
        );

        if (!save_message_files) continue;

        mkdir(DATA_DIR, S_IRWXU);
        mkdir(DATA_DIR "/messages", S_IRWXU);
        std::string output_filename = DATA_DIR "/messages/" + UUID().as_string();
        std::ofstream file(output_filename);
        file.write(buffer, result.length);
        log_print("Saved message to file [", output_filename, "].");
    }

    log_debug("Closed application receiver thread.");
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

        if (!save_message_files) continue;

        mkdir(DATA_DIR, S_IRWXU);
        mkdir(DATA_DIR "/messages", S_IRWXU);
        std::string output_filename = DATA_DIR "/messages/" + UUID().as_string();
        std::ofstream file(output_filename);
        file.write(buffer, result.length);
        log_print("Saved message to file [", output_filename, "].");
    }

    log_debug("Closed application deliver thread.");
}

bool Process::send_message(
    Destination dest,
    MessageData data,
    [[maybe_unused]] std::string cmd_name,
    [[maybe_unused]] std::string data_description
) {
    if (!initialized()) {
        log_print("Could not execute '", cmd_name, "', node is dead.");
        return false;
    }

    std::string node_id = dest.node;
    std::string group_id = dest.group;

    bool success = false;
    if (node_id == BROADCAST_ID) {
        log_info(
            "Executing command '", cmd_name, "', broadcasting ", data_description, " on group '", group_id, "'."
        );
        success = comm->broadcast(group_id, data);
    }
    else {
        log_info(
            "Executing command '", cmd_name, "', sending ", data_description, " to node ", node_id, " on group '", group_id, "'."
        );
        success = comm->send(group_id, node_id, data);
    }

    bool broadcast = node_id == BROADCAST_ID;

    if (success && !broadcast) {
        log_print("Successfully sent message to node ", node_id, " on group '", group_id, "'.");
    }
    else if (success && broadcast) {
        log_print("Successfully broadcasted message on group '", group_id, "'.");
    }
    else if (!success && broadcast) {
        log_print("Could not broadcast message on group '", group_id, "'.");
    }
    else {
        log_error("Could not send message to node ", node_id, " on group '", group_id, "'.");
    }

    return success;
}


std::string format_group(const GroupInfo& g) {
    std::string hex_key = array_to_hex(g.key);
    return format(
        YELLOW "%s" COLOR_RESET " - key: " H_BLACK "%s" COLOR_RESET ", hash: " H_BLACK "%lu" COLOR_RESET "\n",
        g.id.c_str(),
        hex_key.c_str(),
        g.hash
    );
}
void Process::execute_group_list() {
    std::string out;

    auto joined = comm->get_joined_groups();
    if (joined.size()) {
        out += format("Showing %u joined group(s).\n", joined.size());
        for (const auto& [_, g] : joined) {
            out += format_group(g);
        }
    }
    else {
        out += "Did not join any group yet.\n";
    }

    auto available = comm->get_available_groups();
    if (available.size()) {
        out += format("Showing %u available group(s).\n", available.size());
        for (const auto& [_, g] : available) {
            out += format_group(g);
        }
    }
    else {
        out += "No groups are available.";
    }

    log_print(out);
}

void Process::execute_node_list() {
    GroupRegistry& gr = *comm->get_group_registry();
    auto groups = comm->get_registered_groups();
    auto joined_groups = comm->get_joined_groups();
    NodeMap& nodes = gr.get_nodes();

    std::string out;

    out += format("Showing %u node(s).\n", nodes.size());
    
    for (const auto& [_, node] : nodes) {
        Connection& connection = gr.get_connection(node.get_id());

        const char* local_marker = node.is_remote() ? "" : " (local)";

        std::string group_list;
        for (uint64_t hash : node.get_groups()) {
            if (group_list.length()) group_list += ", ";

            if (groups.contains(hash)) {
                std::string id = groups.at(hash).id;
                const char* color = joined_groups.contains(id) ? H_BLUE : "";
                group_list += format("%s%s" COLOR_RESET, color, id.c_str());
            }
            else group_list += format(H_BLACK "%s" COLOR_RESET, std::to_string(hash).c_str());
        }
        std::string groups_str = group_list.size() ? group_list : H_BLACK "none" COLOR_RESET;

        out += format(
            YELLOW "%s" COLOR_RESET "%s - address: " H_BLACK "%s" COLOR_RESET ", state: " H_BLACK "%s" COLOR_RESET ", connection: " H_BLACK "%s" COLOR_RESET ", groups: %s\n",
            node.get_id().c_str(),
            local_marker,
            node.get_address().to_string().c_str(),
            node.get_state_name().c_str(),
            connection.get_current_state_name().c_str(),
            groups_str.c_str()
        );
    }

    log_print(out);
}

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
            init();
            return;
        }

        if (!initialized()) {
            log_print("Node is dead.");
            return;
        }

        if (command.type == CommandType::group_list) {
            execute_group_list();
            return;
        }

        if (command.type == CommandType::node_list) {
            execute_node_list();
            return;
        }

        if (command.type == CommandType::group_join) {
            const GroupJoinCommand* cmd = static_cast<const GroupJoinCommand*>(&command);

            auto joined_groups = comm->get_joined_groups();
            if (joined_groups.contains(cmd->id)) throw std::invalid_argument(
                format("Already joined group '%s'.", cmd->id.c_str())
            );

            auto available_groups = comm->get_available_groups();
            if (!available_groups.contains(cmd->id)) {
                if (!cmd->key.has_value()) throw std::invalid_argument(format(
                    "Group '%s' is not registered and no key was provided.", cmd->id.c_str()
                ));
                comm->register_group(cmd->id, *cmd->key);
            }

            comm->join_group(cmd->id);

            return;
        }

        if (command.type == CommandType::group_leave) {
            const GroupLeaveCommand* cmd = static_cast<const GroupLeaveCommand*>(&command);

            auto joined_groups = comm->get_joined_groups();
            if (!joined_groups.contains(cmd->id)) throw std::invalid_argument(
                format("Cannot leave group '%s'.", cmd->id.c_str())
            );

            comm->leave_group(cmd->id);

            return;
        }

        if (command.type == CommandType::text) {
            const TextCommand* cmd = static_cast<const TextCommand*>(&command);

            const std::string& text = cmd->text;

            send_message(
                cmd->dest,
                {text.c_str(), text.length()},
                cmd->name(),
                format("'%s'", text.c_str())
            );
            return;
        }
        
        if (command.type == CommandType::dummy) {
            const DummyCommand* cmd = static_cast<const DummyCommand*>(&command);

            size_t size = cmd->size;

            char data[size];
            create_dummy_data(data, size);

            send_message(
                cmd->dest,
                {data, size},
                cmd->name(),
                format("%u bytes of dummy data", size)
                );
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
                    cmd->dest,
                    {buffer, size},
                    cmd->name(),
                    format("%u bytes from file '%s'", size, path.c_str())
                );
            }
            return;
        }

        if (command.type == CommandType::kill) {
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

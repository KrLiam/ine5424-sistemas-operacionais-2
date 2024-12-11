#include <cstdint>
#include <vector>
#include <thread>
#include <semaphore>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <mutex>

#include "communication/reliable_communication.h"

namespace fs = std::filesystem;

std::string format_bytes(uint64_t bytes, uint8_t decimals = 255) {
    if (bytes < 1024) return format("%uB", bytes);

    std::string p = decimals == 255 ? "%f" : std::string("%0.") + std::to_string(decimals) + "f";

    double kiB = (double) bytes / 1024.0;
    if (kiB < 1024.0) return format(p+"KiB", kiB);

    double miB = (double) kiB / 1024.0;
    if (miB < 1024.0) return format(p+"MiB", miB);

    double giB = (double) miB / 1024.0;
    return format(p+"GiB", giB);
}


enum HashMapOperation : uint16_t {
    READ = 0,
    WRITE = 1,
    READ_RESULT = 2
};

struct HashMapMessage {
    HashMapOperation operation;
    uint16_t key;

    static constexpr uint32_t VALUE_SIZE = Message::MAX_SIZE - sizeof(key) - sizeof(operation);
    unsigned char value[VALUE_SIZE];
};

struct LogEntry {
    uint64_t time;
    size_t bytes;
    uint32_t key;
    bool success = true;
};

struct Worker {
    const Config config;
    const std::string node_id;
    const std::string group_id;
    const double bytes_to_send;
    const IntRange interval_range;
    const uint32_t max_message_size;

    std::thread sender_thread;
    std::thread receiver_thread;
    std::thread deliver_thread;
    std::unique_ptr<ReliableCommunication> comm;

    std::binary_semaphore done_sem{0};
    bool done = false;
    bool terminate = false;

    std::binary_semaphore benchmark_over{0};
    std::binary_semaphore read_sem{0};

    uint64_t start_time;
    double remaining_bytes;
    uint32_t failures;
    std::vector<LogEntry> in_logs;
    std::vector<LogEntry> out_logs;

    uint32_t max_read_operations;
    uint32_t read_operations;
    uint32_t max_write_operations;
    uint32_t write_operations;

    // o numero maximo de chaves na hash table está relacionado com
    // o numero total de nós e a quantidade de ram disponível pro benchmark. 
    static const uint32_t MAX_KEY = 100;
    std::uniform_int_distribution<> key_dis;
    std::unordered_map<uint16_t, std::array<char, HashMapMessage::VALUE_SIZE>> hash_map;
    std::mutex hm_mutex;

    Worker(
        const Config& config,
        const std::string& node_id,
        const std::string& group_id,
        double bytes_to_send,
        IntRange interval_range,
        uint32_t max_message_size,
        uint32_t max_read_operations,
        uint32_t max_write_operations
    ) :
        config(config),
        node_id(node_id),
        group_id(group_id),
        bytes_to_send(bytes_to_send),
        interval_range(interval_range),
        max_message_size(std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE)),
        remaining_bytes(bytes_to_send),
        key_dis(0, MAX_KEY),
        read_operations(0),
        max_read_operations(max_read_operations),
        write_operations(0),
        max_write_operations(max_write_operations)
    {
        initialize_hashmap();
    }

    ~Worker() {
        comm.reset();
        if (sender_thread.joinable()) sender_thread.join();
        if (receiver_thread.joinable()) receiver_thread.join();
        if (deliver_thread.joinable()) deliver_thread.join();
    }

    void initialize_hashmap() {
        for (uint32_t i = 0; i < Worker::MAX_KEY; i++) hash_map[i] = {};
    }

    double bytes_received(uint64_t interval_ms) {
        return bytes_received(interval_ms, DateUtils::now());
    }
    uint64_t bytes_received(uint64_t interval_ms, uint64_t time) {
        if (in_logs.empty()) return 0;

        uint64_t min_time = time - interval_ms;
        int32_t max_i = (int32_t) in_logs.size() - 1;

        if (in_logs[max_i].time < min_time) return 0;

        while (max_i > 0 && in_logs[max_i].time > time) max_i--;

        int32_t min_i = max_i;
        while (min_i > 0 && in_logs[min_i - 1].time > min_time) min_i--;
        
        auto begin = in_logs.begin() + min_i;
        auto end = in_logs.begin() + max_i;

        uint64_t bytes = 0;
        for (auto entry = begin; entry <= end; entry++) {
            bytes += entry->bytes;
        }
        return bytes;
    }

    double bytes_sent(bool success) {
        uint64_t interval = DateUtils::now() - start_time;
        return bytes_sent(success, interval, DateUtils::now());
    }
    double bytes_sent(bool success, uint64_t interval_ms) {
        return bytes_sent(success, interval_ms, DateUtils::now());
    }
    uint64_t bytes_sent(bool success, uint64_t interval_ms, uint64_t time) {
        if (out_logs.empty()) return 0;

        uint64_t min_time = time - interval_ms;
        int32_t max_i = (int32_t) out_logs.size() - 1;

        if (out_logs[max_i].time < min_time) return 0;

        while (max_i > 0 && out_logs[max_i].time > time) max_i--;

        int32_t min_i = max_i;
        while (min_i > 0 && out_logs[min_i - 1].time > min_time) min_i--;
        
        auto begin = out_logs.begin() + min_i;
        auto end = out_logs.begin() + max_i;

        uint64_t bytes = 0;
        for (auto entry = begin; entry <= end; entry++) {
            if (entry->success != success) continue;
            bytes += entry->bytes;
        }
        return bytes;
    }

    void send_routine() {
        HashMapMessage msg;

        while (true) {
            std::size_t size = create_hashmap_message(&msg);

            if (!size) break;

            if (msg.operation == HashMapOperation::READ) {
                std::string node_id = choose_random_node();
                // log_print("Node ", node_id, " is reading ", msg.key, " at ", node_id, ".");

                bool success = comm->send(group_id, node_id, {(const char*) &msg, size});
                if (!success) continue;
                out_logs.push_back({
                    time : DateUtils::now(),
                    bytes : size,
                    key : msg.key
                });
                remaining_bytes -= size;

                read_sem.acquire();
                read_operations++;
            }
            else if (msg.operation == HashMapOperation::WRITE) {
                // log_print("Node ", node_id, " is sending ", size, " bytes (key=", msg.key, ").");
                uint32_t interval = interval_range.random();
                if (interval) std::this_thread::sleep_for(std::chrono::milliseconds(interval));

                bool success = comm->broadcast(group_id, {(const char*) &msg, size});
                out_logs.push_back({
                    time : DateUtils::now(),
                    bytes : size,
                    key : msg.key,
                    success: success
                });

                if (!success) continue;
                remaining_bytes -= size;
                write_operations++;
            }
        }

        // log_print("Node ", node_id, " is done, waiting for benchmark being over.");
        done = true;
        done_sem.release();
        benchmark_over.acquire();

        //log_print("Node ", node_id, " is terminating.");
        terminate = true;
    }

    std::string choose_random_node() {
        uint32_t i = rc_random::dis32(rc_random::gen) % config.nodes.size();
        const NodeConfig& node = config.nodes.at(i);
        return node.id;
    }

    std::size_t create_hashmap_message(HashMapMessage* buffer) {
        HashMapMessage msg;

        uint32_t bytes = std::min(remaining_bytes, (double) max_message_size);
        // remaining_bytes -= bytes;

        uint32_t metadata_bytes = sizeof(msg.operation) + sizeof(msg.key);
        if (bytes < metadata_bytes) return 0;

        // TODO: talvez tirar isso e deixar mandar um pacote cheio pra não bugar o valor no ultimo broadcast
        uint32_t value_size = bytes - metadata_bytes;

        bool can_read = read_operations < max_read_operations;
        bool can_write = write_operations < max_write_operations;
        if (can_read && can_write) msg.operation = (HashMapOperation)rc_random::dis1(rc_random::gen);
        else if (can_read) msg.operation = HashMapOperation::READ;
        else if (can_write) msg.operation = HashMapOperation::WRITE;
        else return 0;

        msg.key = key_dis(rc_random::gen);

        if (msg.operation == HashMapOperation::WRITE) {
            // talvez seja mais interessante ler o valor atual da chave e calcular
            // um novo valor que dependa do anterior e da ordem que estas escritas são feitas
            // para verificar a integridade do ab
            for (uint32_t i = 0; i < value_size; i++) {
                uint8_t byte = rc_random::dis8(rc_random::gen);
                msg.value[i] = byte;
            }
        }

        memcpy(buffer, &msg, sizeof(msg));
        
        return bytes;
    }

    void receive_routine() {
        HashMapMessage msg;
        ReceiveResult result;

        while (!terminate) {
            try {
                result = comm->receive((char*) &msg);
            }
            catch (const buffer_termination& err) {
                break;
            }

            if (terminate) break;

            LogEntry entry = {
                time : DateUtils::now(),
                bytes : result.length,
                key : msg.key
            };
            in_logs.push_back(entry);

            if (msg.operation == HashMapOperation::READ) {
                // log_print("Node ", node_id, " received read request on group ", result.group_id, " (key=", msg.key, ").");
                HashMapMessage answer = create_read_answer(msg);
                bool success = comm->send(result.group_id, result.sender_id, {(char*)&answer, sizeof(answer)});
                if (!success) continue;
                out_logs.push_back({
                    time : DateUtils::now(),
                    bytes : sizeof(answer),
                    key : msg.key
                });
            }
            else if (msg.operation == HashMapOperation::READ_RESULT) {
                // log_print(msg.key, " on node ", result.sender_id, " is ", msg.value, ".");
                read_sem.release();
            }
        }
    }

    HashMapMessage create_read_answer(HashMapMessage& read_msg) {
        HashMapMessage answer;

        answer.operation = HashMapOperation::READ_RESULT;
        answer.key = read_msg.key;

        hm_mutex.lock();
        memcpy(answer.value, hash_map[read_msg.key].data(), HashMapMessage::VALUE_SIZE);
        hm_mutex.unlock();

        return answer;
    }

    void deliver_routine() {
        HashMapMessage msg;
        ReceiveResult result;

        while (!terminate) {
            try {
                result = comm->deliver((char*) &msg);
            }
            catch (const buffer_termination& err) {
                break;
            }

            if (terminate) break;

            // log_print("Node ", node_id, " received ", result.length, " bytes on group ", result.group_id, " (key=", msg.key, ").");

            LogEntry entry = {
                time : DateUtils::now(),
                bytes : result.length,
                key : msg.key
            };
            in_logs.push_back(entry);

            hm_mutex.lock();
            memcpy(hash_map[msg.key].data(), msg.value, result.length - sizeof(msg.operation) - sizeof(msg.key));
            hm_mutex.unlock();
        }
    }

    void start() {
        comm = std::make_unique<ReliableCommunication>(node_id, Message::MAX_SIZE, false, config);
        if (group_id != GLOBAL_GROUP_ID) comm->join_group(group_id);

        start_time = DateUtils::now();
        sender_thread = std::thread([this]() { send_routine(); });
        receiver_thread = std::thread([this]() { receive_routine(); });
        deliver_thread = std::thread([this]() { deliver_routine(); });
    }

    void wait() {
        if (sender_thread.joinable()) sender_thread.join();
    }
};


std::string indent(std::string str, std::string indentation) {
    Reader reader(str);
    auto ovr = reader.override_whitespace(false);

    std::string result = indentation;

    while (char ch = reader.read()) {
        result += ch;
        if (ch == '\n') result += indentation;
    }

    return result;
}

std::string serialize_double(double value) {
    if(isinfl(value)) return "1e9999";

    return std::to_string(value);
}

struct BenchmarkParameters {
    uint32_t total_groups;
    uint32_t total_nodes_in_group;
    double bytes_sent_per_node;
    IntRange interval_range;
    uint32_t max_message_size;
    BenchmarkMode mode;
    bool disable_encryption;
    bool disable_checksum;
    uint32_t max_read_operations;
    uint32_t max_write_operations;
    uint32_t alive;
    uint32_t max_inactivity_time;
    bool measure_failures;

    std::string serialize() {
        std::string contents;

        contents += format("\n\"num_groups\": %u,", total_groups);
        contents += format("\n\"num_nodes\": %u,", total_nodes_in_group);
        contents += format("\n\"bytes_per_node\": %f,", bytes_sent_per_node);
        contents += format("\n\"interval\": [%u, %u],", interval_range.min, interval_range.max);
        contents += format("\n\"max_message_size\": %u,", max_message_size);
        contents += format("\n\"max_read_operations\": %u,", max_read_operations);
        contents += format("\n\"max_write_operations\": %u,", max_write_operations);
        contents += format("\n\"mode\": \"%s\",", benchmark_mode_to_string(mode));
        contents += format("\n\"encryption\": %s,", disable_encryption ? "false" : "true");
        contents += format("\n\"checksum\": %s,", disable_checksum ? "false" : "true");
        contents += format("\n\"alive\": %u,", alive);
        contents += format("\n\"max_inactivity_time\": %u", max_inactivity_time);

        return std::string("{") + indent(contents, "    ") + "\n}";
    }

    std::string to_string() {
        std::string out = format(
            "Groups=%u\n"
            "Nodes=%u per group (%u total)\n"
            "Bytes=%s sent per node (%s total)\n"
            "Interval=%sms\n"
            "Max Message Size=%u\n"
            "Max Read Operations=%u\n"
            "Max Write Operations=%u\n"
            "Message Mode=%s\n"
            "Encryption=%s\n"
            "Checksum=%s\n"
            "Alive=%u\n"
            "Max Inactivity Time=%u",
            total_groups,
            total_nodes_in_group,
            total_groups*total_nodes_in_group,
            format_bytes(bytes_sent_per_node).c_str(),
            format_bytes(bytes_sent_per_node*total_groups*total_nodes_in_group).c_str(),
            interval_range.to_string().c_str(),
            max_message_size,
            max_read_operations,
            max_write_operations,
            benchmark_mode_to_string(mode),
            disable_encryption ? "disabled" : "enabled",
            disable_checksum ? "disabled" : "enabled",
            alive,
            max_inactivity_time
        );
        return out;
    }
};

struct BenchmarkSnapshot {
    double elapsed_time;
    double transferred_bytes;
    double total_bytes;
    double failed_bytes;
    double progress_ratio;
    double time_estimate;
    double avg_in_throughput;
    double out_throughput;
    double failure_out_throughput;
    uint32_t read_operations;
    uint32_t write_operations;
    bool measure_failures;

    std::string serialize() {
        return format(
            "{"
            "\"elapsed_time\": %f, "
            "\"transferred_bytes\": %f, "
            "\"total_bytes\": %f, "
            "%s"
            "\"progress_ratio\": %f, "
            "\"time_estimate\": %s, "
            "\"avg_in\": %f, "
            "\"total_out\": %f, "
            "\"total_failure_out\": %f, "
            "\"read_operations\": %u, "
            "\"write_operations\": %u"
            "}",
            elapsed_time,
            transferred_bytes,
            total_bytes,
            measure_failures ? format("\"failed_bytes\": %u, ", failed_bytes).c_str() : "",
            progress_ratio,
            serialize_double(time_estimate).c_str(),
            avg_in_throughput,
            out_throughput,
            failure_out_throughput,
            read_operations,
            write_operations
        );
    }

    std::string to_string() {
        return format(
            "%.1fs | "
            "%s/%s (%.5f%%)%s, "
            "time_left=%.1fs, "
            "avg_in=%s/s, "
            "total_out=%s/s%s",
            elapsed_time,
            format_bytes(transferred_bytes, 3).c_str(),
            format_bytes(total_bytes, 3).c_str(),
            measure_failures ?  format(" (%s failed)", format_bytes(failed_bytes, 3).c_str()).c_str() : "",
            progress_ratio*100,
            time_estimate,
            format_bytes(avg_in_throughput, 3).c_str(),
            format_bytes(out_throughput, 3).c_str(),
            measure_failures ? format(" (failing %s/s)", format_bytes(failure_out_throughput, 3).c_str()).c_str() : ""
        );
    }
};

struct BenchmarkResult {
    bool completed = false;
    uint64_t start_time;
    BenchmarkParameters params;
    std::vector<BenchmarkSnapshot> snapshots;

    static constexpr const char* directory_path = ".data/benchmark";

    std::string serialize() {
        std::string contents;
        contents += format("\n\"start_time\": %u,", start_time);

        contents += format("\n\"params\": %s,", params.serialize().c_str());

        std::string snapshots_contents;
        for (size_t i = 0; i < snapshots.size(); i++) {
            auto& snapshot = snapshots.at(i);
            bool last = i == (snapshots.size() - 1);

            snapshots_contents += "\n" + snapshot.serialize();
            if (!last) snapshots_contents += ",";
        }
        contents += format("\n\"snapshots\": [%s\n]", indent(snapshots_contents, "    ").c_str());

        return std::string("{") + indent(contents, "    ") + "\n}";
    }

    std::string produce_filename(fs::path directory) {
        std::string value;

        value += format("%ug", params.total_groups);
        value += format("-%un", params.total_nodes_in_group);
        value += format("-%s", format_bytes(params.bytes_sent_per_node, 3).c_str());
        value += format("-%s", benchmark_mode_to_string(params.mode));
        if (params.interval_range.max > 0) value += format("-%sms", params.interval_range.to_string().c_str());
        if (params.max_message_size != PacketData::MAX_MESSAGE_SIZE)
            value += format("-%smax", format_bytes(params.max_message_size, 3).c_str());
        if (params.max_read_operations) value += format("-%ur", params.max_read_operations);
        if (params.max_write_operations) value += format("-%uw", params.max_write_operations);
        if (params.disable_checksum) value += "-nochecksum";
        if (params.disable_encryption) value += "-noencryption";

        uint32_t i = 1;
        while (fs::exists(format("%s/%s-%u.json", directory.c_str(), value.c_str(), i))) i++;
        value += format("-%u.json", i);
        
        return value;
    }

    std::optional<std::string> save(std::string file_path) {
        fs::path directory = directory_path;
        fs::path path = file_path;

        if (!file_path.length()) {
            std::string filename = produce_filename(directory);
            path = format(".data/benchmark/%s", filename.c_str());
        }
        else if (fs::is_directory(path)) {
            std::string filename = produce_filename(path);
            path = format("%s/%s", file_path.c_str(), filename.c_str());
        }

        if (!path.has_extension()) path += ".json";

        std::string value = serialize();

        fs::path parent_path = path.parent_path();
        if (!parent_path.empty()) fs::create_directories(parent_path);

        std::ofstream file(path);
        if (!file.is_open()) return std::nullopt;
        file << value;
        file.close();

        return path;
    }
};

class Benchmarker {
    BenchmarkParameters params;

    bool measure_failures;
    bool global_group = false;
    uint64_t start_time;
    std::vector<std::unique_ptr<Worker>> workers;


public:
    Benchmarker(
        uint32_t total_groups,
        uint32_t total_nodes_in_group,
        double bytes_sent_per_node,
        IntRange interval_range,
        uint32_t max_message_size,
        BenchmarkMode mode,
        bool no_encryption,
        bool no_checksum,
        uint32_t max_read_operations,
        uint32_t max_write_operations,
        uint32_t alive,
        uint32_t max_inactivity_time,
        bool measure_failures
    ) :
        measure_failures(measure_failures)
    {
        global_group = total_groups == 0;
        total_groups = total_groups ? total_groups : 1;

        params = {
            total_groups : total_groups,
            total_nodes_in_group : total_nodes_in_group,
            bytes_sent_per_node : bytes_sent_per_node,
            interval_range : interval_range,
            max_message_size : std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE),
            mode : mode,
            disable_encryption : no_encryption,
            disable_checksum : no_checksum,
            max_read_operations : max_read_operations,
            max_write_operations : max_write_operations,
            alive : alive,
            max_inactivity_time : max_inactivity_time,
            measure_failures : measure_failures
        };
        workers.reserve(total_nodes());
    }

    uint32_t total_nodes() { return params.total_groups * params.total_nodes_in_group; }

    std::unordered_map<std::string, ByteArray> create_groups() {
        std::unordered_map<std::string, ByteArray> map;

        if (global_group) {
            map.emplace(GLOBAL_GROUP_ID, ByteArray{0});
            return map;
        }

        for (uint32_t i = 0; i < params.total_groups; i++) {
            std::string id = format("group_%u", i);
            ByteArray key;
            for (int k = 0; k < 256; k++) {
                key.push_back(i); // fazer ser um numero aleatorio
            }

            map.emplace(id, key);
        }

        return map;
    }

    std::vector<NodeConfig> create_nodes() {
        std::vector<NodeConfig> nodes;

        for (uint32_t i = 0; i < total_nodes(); i++) {
            std::string id = std::to_string(i);

            uint16_t port = 3000 + i; // fazer um esquema de verificar se esta porta está disponível
            SocketAddress address = {{127, 0, 0, 1}, port};

            NodeConfig node = {id, address};
            nodes.push_back(node);
        }

        return nodes;
    }

    Config create_config() {
        Config config;

        config.groups = create_groups();
        config.nodes = create_nodes();
        config.alive = params.alive;
        config.broadcast =
            params.mode == BenchmarkMode::URB ? BroadcastType::URB :
            params.mode == BenchmarkMode::AB ? BroadcastType::AB :
            BroadcastType::BEB;

        config.faults = {delay: {0, 0}, lose_chance: 0, corrupt_chance: 0};
        config.disable_encryption = params.disable_encryption;
        config.disable_checksum = params.disable_checksum;

        return config;
    }

    BenchmarkResult run() {
        BenchmarkResult result;
        result.params = params;

        workers.clear();

        Config config = create_config();

        int i = 0;
        for (const auto& [group_id, _] : config.groups) {
            Config group_config = config;

            // cada grupo vai ter somente os nós participantes na configuração
            // hack momentaneo para ter um lider em cada grupo
            auto begin_iter = config.nodes.begin() + i * params.total_nodes_in_group;
            i++;
            std::vector<NodeConfig> nodes(begin_iter, begin_iter + params.total_nodes_in_group);
            group_config.nodes = nodes;

            for (const NodeConfig& node : nodes) {
                workers.push_back(
                    std::make_unique<Worker>(
                        group_config,
                        node.id,
                        group_id,
                        params.bytes_sent_per_node,
                        params.interval_range,
                        params.max_message_size,
                        params.max_read_operations,
                        params.max_write_operations
                    )
                );
            }
        }

        std::cout << "Starting benchmark.\n"
            << params.to_string() << "\n" << std::endl;

        start_time = DateUtils::now();
        result.start_time = start_time;

        for (auto& worker : workers) {
            worker->start();
        }

        uint64_t last_activity_time = start_time;
        while (true) {
            BenchmarkSnapshot snapshot = create_snapshot();
            result.snapshots.push_back(snapshot);

            uint64_t now = DateUtils::now();
            if (snapshot.out_throughput) {
                last_activity_time = now;
            }
            if ( (now - last_activity_time) > params.max_inactivity_time ) {
                log_error("Aborting benchmark due to inactivity. Terminating it ungracefully.");
                return result;
            }

            std::cout << snapshot.to_string() << std::endl;

            if (all_done()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        for (auto& worker : workers) {
            worker->done_sem.acquire();
        }

        log_print("Benchmark is over.");
        result.completed = true;

        for (auto& worker : workers) {
            worker->benchmark_over.release();
            worker->wait();
        }

        validate_hashmaps_match(config);

        return result;
    }

    void validate_hashmaps_match(const Config& config) {
        log_print("Validating if hashmaps match between all workers that share a group.");

        for (const auto &[group_id, _] : config.groups) {
            std::unordered_map<uint32_t, std::array<char, HashMapMessage::VALUE_SIZE>> group_hm;

            for (auto &worker : workers) {
                if (worker->group_id != group_id) continue;
                for (uint32_t key = 0; key <= Worker::MAX_KEY; key++) {
                    if (!group_hm.contains(key)) {
                        group_hm[key] = worker->hash_map[key];
                        continue;
                    }
                    if (group_hm[key] != worker->hash_map[key]) {
                        log_print("Hashmap of group ", group_id, " does not match. ");
                        // log_print(group_hm[key].data(), " != ", worker->hash_map[key].data());
                        return;
                    }
                }
            }

            log_print("Hashmap of group ", group_id, " is the same in all nodes.");
        }
    }

    bool all_done() {
        for (auto& worker : workers) {
            if (!worker->done) return false;
        }
        return true;
    }

    BenchmarkSnapshot create_snapshot() {
        uint64_t now = DateUtils::now();
        double elapsed = (double)(now - start_time) / 1000.0;

        double total_bytes = total_nodes() * params.bytes_sent_per_node;
        double remaining_bytes = 0;
        double failed_bytes = 0;
        double avg_in_throughput = 0;
        double out_throughput = 0;
        double failure_out_throughput = 0;
        uint32_t read_operations = 0;
        uint32_t write_operations = 0;

        for (auto& worker : workers) {
            avg_in_throughput += worker->bytes_received(1000); // bytes recebidos no ultimo segundo
            remaining_bytes += worker->remaining_bytes;
            failed_bytes += worker->bytes_sent(false);
            out_throughput += worker->bytes_sent(true, 1000);
            failure_out_throughput += worker->bytes_sent(false, 1000);
            read_operations += worker->read_operations;
            write_operations += worker->write_operations;
        }
        avg_in_throughput /= params.total_nodes_in_group;

        double transferred_bytes = total_bytes - remaining_bytes;
        double remaining_ratio = (double) remaining_bytes/ (double) total_bytes;
        double progress_ratio = 1 - remaining_ratio;

        double time_estimate = progress_ratio != 0 ? remaining_ratio/progress_ratio * elapsed : std::numeric_limits<double>::infinity();

        return BenchmarkSnapshot{
            elapsed_time : elapsed,
            transferred_bytes : transferred_bytes,
            total_bytes : total_bytes,
            failed_bytes : failed_bytes,
            progress_ratio : progress_ratio,
            time_estimate : time_estimate,
            avg_in_throughput : avg_in_throughput,
            out_throughput : out_throughput,
            failure_out_throughput : failure_out_throughput,
            read_operations : read_operations,
            write_operations : write_operations,
            measure_failures : measure_failures
        };
    }
};

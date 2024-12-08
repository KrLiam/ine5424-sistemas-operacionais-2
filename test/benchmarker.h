#include <cstdint>
#include <vector>
#include <thread>
#include <semaphore>

#include "communication/reliable_communication.h"


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


struct BenchmarkResult {
    // vetor com throughput a cada segundo em cada no, 
};

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
};

struct Worker {
    const Config config;
    const std::string node_id;
    const std::string group_id;
    const uint32_t bytes_to_send;
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

    uint32_t remaining_bytes;
    std::vector<LogEntry> in_logs;
    std::vector<LogEntry> out_logs;

    // o numero maximo de chaves na hash table está relacionado com
    // o numero total de nós e a quantidade de ram disponível pro benchmark. 
    static const uint32_t MAX_KEY = 100;
    std::uniform_int_distribution<> key_dis;
    std::unordered_map<uint16_t, std::array<char, HashMapMessage::VALUE_SIZE>> hash_map;

    Worker(
        const Config& config,
        const std::string& node_id,
        const std::string& group_id,
        uint32_t bytes_to_send,
        IntRange interval_range,
        uint32_t max_message_size
    ) :
        config(config),
        node_id(node_id),
        group_id(group_id),
        bytes_to_send(bytes_to_send),
        interval_range(interval_range),
        max_message_size(std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE)),
        remaining_bytes(bytes_to_send),
        key_dis(0, MAX_KEY)
    {}

    ~Worker() {
        comm.reset();
        if (sender_thread.joinable()) sender_thread.join();
        if (receiver_thread.joinable()) receiver_thread.join();
        if (deliver_thread.joinable()) deliver_thread.join();
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

    double bytes_sent(uint64_t interval_ms) {
        return bytes_sent(interval_ms, DateUtils::now());
    }
    uint64_t bytes_sent(uint64_t interval_ms, uint64_t time) {
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
            }
            else if (msg.operation == HashMapOperation::WRITE) {
                // log_print("Node ", node_id, " is sending ", size, " bytes (key=", msg.key, ").");
                uint32_t interval = interval_range.random();

                bool success = comm->broadcast(group_id, {(const char*) &msg, size});
                std::this_thread::sleep_for(std::chrono::milliseconds(interval));
                if (!success) continue;
                out_logs.push_back({
                    time : DateUtils::now(),
                    bytes : size,
                    key : msg.key
                });
                remaining_bytes -= size;
            }
        }

        //log_print("Node ", node_id, " is done, waiting for benchmark being over.");
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

        uint32_t bytes = std::min(remaining_bytes, max_message_size);
        // remaining_bytes -= bytes;

        uint32_t metadata_bytes = sizeof(msg.operation) + sizeof(msg.key);
        if (bytes < metadata_bytes) return 0;

        // TODO: talvez tirar isso e deixar mandar um pacote cheio pra não bugar o valor no ultimo broadcast
        uint32_t value_size = bytes - metadata_bytes;

        // apenas escrita por enquanto
        msg.operation = (HashMapOperation)rc_random::dis1(rc_random::gen);;

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
                return;
            }

            if (terminate) return;

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

        if (hash_map.contains(read_msg.key)) {
            memcpy(answer.value, hash_map[read_msg.key].data(), HashMapMessage::VALUE_SIZE);
        }
        else {
            std::string undefined = "undefined";
            memcpy(answer.value, undefined.c_str(), undefined.size());
        }

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
                return;
            }

            if (terminate) return;

            // log_print("Node ", node_id, " received ", result.length, " bytes on group ", result.group_id, " (key=", msg.key, ").");

            LogEntry entry = {
                time : DateUtils::now(),
                bytes : result.length,
                key : msg.key
            };
            in_logs.push_back(entry);

            if (!hash_map.contains(msg.key)) hash_map[msg.key] = {};
            memcpy(hash_map[msg.key].data(), msg.value, result.length - sizeof(msg.operation) - sizeof(msg.key));
        }
    }

    void start() {
        comm = std::make_unique<ReliableCommunication>(node_id, Message::MAX_SIZE, false, config);
        if (group_id != GLOBAL_GROUP_ID) comm->join_group(group_id);

        sender_thread = std::thread([this]() { send_routine(); });
        receiver_thread = std::thread([this]() { receive_routine(); });
        deliver_thread = std::thread([this]() { deliver_routine(); });
    }

    void wait() {
        if (sender_thread.joinable()) sender_thread.join();
    }
};

class Benchmarker {
    uint32_t total_groups;
    uint32_t total_nodes_in_group;
    uint32_t bytes_sent_per_node;
    IntRange interval_range;
    uint32_t max_message_size;
    BenchmarkMode mode;

    bool global_group = false;
    uint64_t start_time;
    std::vector<std::unique_ptr<Worker>> workers;

    bool disable_encryption;
    bool disable_checksum;

public:
    Benchmarker(
        uint32_t total_groups,
        uint32_t total_nodes_in_group,
        uint32_t bytes_sent_per_node,
        IntRange interval_range,
        uint32_t max_message_size,
        BenchmarkMode mode,
        bool no_encryption,
        bool no_checksum
    ) :
        total_nodes_in_group(total_nodes_in_group),
        bytes_sent_per_node(bytes_sent_per_node),
        interval_range(interval_range),
        max_message_size(std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE)),
        mode(mode),
        disable_encryption(no_encryption),
        disable_checksum(no_checksum)
    {
        global_group = total_groups == 0;
        this->total_groups = total_groups ? total_groups : 1;
        workers.reserve(total_nodes());
    }

    uint32_t total_nodes() { return total_groups * total_nodes_in_group; }

    std::unordered_map<std::string, ByteArray> create_groups() {
        std::unordered_map<std::string, ByteArray> map;

        if (global_group) {
            map.emplace(GLOBAL_GROUP_ID, ByteArray{0});
            return map;
        }

        for (uint32_t i = 0; i < total_groups; i++) {
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
        config.alive = 100;
        config.broadcast =
            mode == BenchmarkMode::URB ? BroadcastType::URB :
            mode == BenchmarkMode::AB ? BroadcastType::AB :
            BroadcastType::BEB;

        config.faults = {delay: {0, 0}, lose_chance: 0, corrupt_chance: 0};
        config.disable_encryption = disable_encryption;
        config.disable_checksum = disable_checksum;

        return config;
    }

    BenchmarkResult run() {
        workers.clear();

        Config config = create_config();

        int i = 0;
        for (const auto& [group_id, _] : config.groups) {
            Config group_config = config;

            // cada grupo vai ter somente os nós participantes na configuração
            // hack momentaneo para ter um lider em cada grupo
            auto begin_iter = config.nodes.begin() + i * total_nodes_in_group;
            i++;
            std::vector<NodeConfig> nodes(begin_iter, begin_iter + total_nodes_in_group);
            group_config.nodes = nodes;

            for (const NodeConfig& node : nodes) {
                workers.push_back(
                    std::make_unique<Worker>(
                        group_config,
                        node.id,
                        group_id,
                        bytes_sent_per_node,
                        interval_range,
                        max_message_size
                    )
                );
            }
        }

        print_start();

        start_time = DateUtils::now();

        for (auto& worker : workers) {
            worker->start();
        }

        while (true) {
            print_progress();

            if (all_done()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        for (auto& worker : workers) {
            worker->done_sem.acquire();
        }
        
        log_print("Benchmark is over.");

        for (auto& worker : workers) {
            worker->benchmark_over.release();
            worker->wait();
        }

        

        return BenchmarkResult();
    }

    bool all_done() {
        for (auto& worker : workers) {
            if (!worker->done) return false;
        }
        return true;
    }

    void print_start() {
        std::string out = format(
            "Starting benchmark.\n"
            "Groups=%u \n"
            "Nodes=%u per group (%u total)\n"
            "Bytes=%s sent per node (%s total)\n"
            "Interval between messages=%s\n"
            "Max message size=%u\n"
            "Message mode=%s\n\n",
            total_groups,
            total_nodes_in_group,
            total_nodes(),
            format_bytes(bytes_sent_per_node).c_str(),
            format_bytes(bytes_sent_per_node*total_nodes()).c_str(),
            interval_range.to_string().c_str(),
            max_message_size,
            benchmark_mode_to_string(mode)
        );
        std::cout << out;
    }

    void print_progress() {
        uint64_t now = DateUtils::now();
        double elapsed = (double)(now - start_time) / 1000.0;

        double total_bytes = total_nodes() * bytes_sent_per_node;
        double remaining_bytes = 0;
        double avg_in_throughput = 0;
        double out_throughput = 0;

        for (auto& worker : workers) {
            remaining_bytes += worker->remaining_bytes;
            avg_in_throughput += worker->bytes_received(1000); // bytes recebidos no ultimo segundo
            out_throughput += worker->bytes_sent(1000);
        }
        avg_in_throughput /= total_nodes_in_group;

        uint32_t transferred_bytes = total_bytes - remaining_bytes;
        double remaining_ratio = remaining_bytes/total_bytes;
        double progress_ratio = 1 - remaining_ratio;

        double time_estimate = remaining_ratio/progress_ratio * elapsed;


        std::string out = format(
            "%.1fs | "
            "%s/%s (%.5f%%), "
            "time_left=%.1fs, "
            "avg_in=%s/s, "
            "total_out=%s/s",
            elapsed,
            format_bytes(transferred_bytes, 3).c_str(),
            format_bytes(total_bytes, 3).c_str(),
            progress_ratio*100,
            time_estimate,
            format_bytes(avg_in_throughput, 3).c_str(),
            format_bytes(out_throughput, 3).c_str()
        );
        std::cout << out << std::endl;
    }
};

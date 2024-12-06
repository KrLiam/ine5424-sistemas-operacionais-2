#include <cstdint>
#include <vector>
#include <thread>
#include <semaphore>

#include "communication/reliable_communication.h"


struct BenchmarkResult {
    // vetor com throughput a cada segundo em cada no, 
};

enum HashMapOperation : uint16_t {
    READ = 0,
    WRITE = 1
};

struct HashMapMessage {
    HashMapOperation operation;
    uint16_t key;

    static constexpr uint32_t VALUE_SIZE = Message::MAX_SIZE - sizeof(key) - sizeof(operation);
    unsigned char value[VALUE_SIZE];
};

struct Worker {
    const Config config;
    const std::string node_id;
    const std::string group_id;
    const uint32_t bytes_to_send;
    const uint32_t interval_between_messages;
    const uint32_t max_message_size;

    std::thread sender_thread;
    std::thread receiver_thread;
    std::thread deliver_thread;
    std::unique_ptr<ReliableCommunication> comm;

    std::binary_semaphore node_over{0};
    std::binary_semaphore benchmark_over{0};

    std::binary_semaphore read_sem{0};

    uint32_t remaining_bytes;

    Worker(
        const Config& config,
        const std::string& node_id,
        const std::string& group_id,
        uint32_t bytes_to_send,
        uint32_t interval_between_messages,
        uint32_t max_message_size
    ) :
        config(config),
        node_id(node_id),
        group_id(group_id),
        bytes_to_send(bytes_to_send),
        interval_between_messages(interval_between_messages),
        max_message_size(std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE)),
        remaining_bytes(bytes_to_send)
    {}

    ~Worker() {
        comm.reset();
        if (sender_thread.joinable()) sender_thread.join();
        if (receiver_thread.joinable()) receiver_thread.join();
        if (deliver_thread.joinable()) deliver_thread.join();
    }

    void send_routine() {
        HashMapMessage msg;

        while (true) {
            std::size_t size = create_hashmap_message(&msg);

            if (!size) break;

            if (msg.operation == HashMapOperation::READ) {
                std::string node_id = choose_random_node();
                bool success = comm->send(group_id, node_id, {(const char*) &msg, size});
                if (!success) continue;
                read_sem.acquire(); // TODO: dar release no receive quando o resultado da leitura chegar
            }
            else if (msg.operation == HashMapOperation::WRITE) {
                log_print("Node ", node_id, " is sending ", size, " bytes (key=", msg.key, ").");
                comm->broadcast(group_id, {(const char*) &msg, size});
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_between_messages));
            }
        }

        log_print("Node ", node_id, " is done, waiting for benchmark being over.");
        node_over.release();
        benchmark_over.acquire();
        log_print("Node ", node_id, " is terminating.");
    }

    std::string choose_random_node() {
        uint32_t i = rc_random::dis32(rc_random::gen) % config.nodes.size();
        const NodeConfig& node = config.nodes.at(i);
        return node.id;
    }

    std::size_t create_hashmap_message(HashMapMessage* buffer) {
        HashMapMessage msg;

        uint32_t bytes = std::min(remaining_bytes, max_message_size);
        remaining_bytes -= bytes;

        uint32_t metadata_bytes = sizeof(msg.operation) + sizeof(msg.key);
        if (bytes < metadata_bytes) return 0;

        uint32_t value_size = bytes - metadata_bytes;

        // apenas escrita por enquanto
        msg.operation = HashMapOperation::WRITE;

        // o numero maximo de chaves na hash table está relacionado com
        // o numero total de nós e a quantidade de ram disponível pro benchmark. 
        uint32_t max_key = 100;
        std::uniform_int_distribution<> key_dis(0, max_key);
        msg.key = key_dis(rc_random::gen);

        // talvez seja mais interessante ler o valor atual da chave e calcular
        // um novo valor que dependa do anterior e da ordem que estas escritas são feitas
        // para verificar a integridade do ab
        for (uint32_t i = 0; i < value_size; i++) {
            uint8_t byte = rc_random::dis8(rc_random::gen);
            msg.value[i] = byte;
        }

        memcpy(buffer, &msg, sizeof(msg));
        
        return bytes;
    }

    void receive_routine() {
        // TODO: aguardar receber o resultado de um read e dar release no read_sem
    }

    void deliver_routine() {
        HashMapMessage msg;
        ReceiveResult result;

        while (true) {
            try {
                result = comm->deliver((char*) &msg);
            }
            catch (const buffer_termination& err) {
                return;
            }

            log_print("Node ", node_id, " received ", result.length, " bytes on group ", result.group_id, " (key=", msg.key, ").");

            // provavelmente pegar o tempo atual e dar push no vetor
            // para calcular o throughput posteriormente, além de
            // fazer a logica de escrever na hash table.
        }
    }

    void start() {
        comm = std::make_unique<ReliableCommunication>(node_id, Message::MAX_SIZE, false, config);
        comm->join_group(group_id);

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
    uint32_t interval_between_messages;
    uint32_t max_message_size;

    std::vector<std::unique_ptr<Worker>> workers;

public:
    Benchmarker(
        uint32_t total_groups,
        uint32_t total_nodes_in_group,
        uint32_t bytes_sent_per_node,
        uint32_t interval_between_messages,
        uint32_t max_message_size
    ) :
        total_groups(total_groups),
        total_nodes_in_group(total_nodes_in_group),
        bytes_sent_per_node(bytes_sent_per_node),
        interval_between_messages(interval_between_messages),
        max_message_size(std::clamp(max_message_size, (uint32_t) 1, (uint32_t) Message::MAX_SIZE))
    {
        workers.reserve(total_nodes());
    }

    uint32_t total_nodes() { return total_groups * total_nodes_in_group; }

    std::unordered_map<std::string, ByteArray> create_groups() {
        std::unordered_map<std::string, ByteArray> map;

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
        config.broadcast = BroadcastType::AB;
        config.faults = {delay: {0, 0}, lose_chance: 0, corrupt_chance: 0};

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
                        interval_between_messages,
                        max_message_size
                    )
                );
            }
        }

        for (auto& worker : workers) {
            worker->start();
        }

        for (auto& worker : workers) {
            worker->node_over.acquire();
        }
        
        log_print("Benchmark is over.");

        for (auto& worker : workers) {
            worker->benchmark_over.release();
            worker->wait();
        }

        return BenchmarkResult();
    }
};
#include <cstdint>
#include <vector>
#include <thread>
#include <semaphore>

#include "communication/reliable_communication.h"


struct BenchmarkResult {
    // vetor com throughput a cada segundo em cada no, 
};

enum HashMapOperation : bool {
    READ = 0,
    WRITE = 1
};

struct HashMapMessage {
    HashMapOperation operation;
    unsigned char key[4];
    unsigned char value[Message::MAX_SIZE - sizeof(key) - sizeof(operation)]
};

namespace hash_map_message {
    bool is_read(const char* msg) {
        return msg[0];
    }
}

struct Worker {
    const Config& config;
    const std::string node_id;
    const std::string group_id;
    const uint32_t bytes_sent;
    const uint32_t messages_sent;

    std::thread sender_thread;
    std::thread receiver_thread;
    std::thread deliver_thread;
    std::unique_ptr<ReliableCommunication> comm;

    std::binary_semaphore read_sem{0};

    Worker(
        const Config& config,
        std::string node_id,
        std::string group_id,
        uint32_t bytes_sent,
        uint32_t messages_sent
    ) :
        config(config),
        node_id(node_id),
        group_id(group_id),
        bytes_sent(bytes_sent),
        messages_sent(messages_sent)
    {}

    ~Worker() {
        comm.reset();
        if (sender_thread.joinable()) sender_thread.join();
        if (receiver_thread.joinable()) receiver_thread.join();
        if (deliver_thread.joinable()) deliver_thread.join();
    }

    void send_routine() {
        char buffer[Message::MAX_SIZE];

        while (true) {
            std::size_t size = create_hashmap_message(buffer);
            if (hash_map_message::is_read(buffer)) {
                std::string node_id = choose_random_node();
                bool success = comm->send(group_id, node_id, {buffer, size});
                if (!success) continue;
                read_sem.acquire(); // TODO: dar release no receive quando o resultado da leitura chegar
            }
            else {
                comm->broadcast(group_id, {buffer, size});
            }
        }
    }

    std::string choose_random_node() {

    }

    std::size_t create_hashmap_message(const char *buffer) {
        // criar msg de write ou read aleatoriamente
    }

    void receive_routine() {
        // TODO: aguardar receber o resultado de um read e dar release no read_sem
    }

    void deliver_routine() {
        char buffer[Message::MAX_SIZE];
        ReceiveResult result;

        while (true) {
            try {
                result = comm->deliver(buffer);
            }
            catch (const buffer_termination& err) {
                return;
            }

            // provavelmente pegar o tempo atual e dar push no vetor
            // para calcular o throughput posteriormente, além de
            // fazer a logica de escrever na hash table.
        }
    }

    void start() {
        comm = std::make_unique<ReliableCommunication>(node_id, Message::MAX_SIZE, false, config);
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
    uint32_t messages_sent_per_node;

    std::vector<std::unique_ptr<Worker>> workers;

public:
    Benchmarker(
        uint32_t total_groups,
        uint32_t total_nodes_in_group,
        uint32_t bytes_sent_per_node,
        uint32_t messages_sent_per_node
    ) :
        total_groups(total_groups),
        total_nodes_in_group(total_nodes_in_group),
        bytes_sent_per_node(bytes_sent_per_node),
        messages_sent_per_node(messages_sent_per_node)
    {
    }

    std::unordered_map<std::string, ByteArray> create_groups() {
        std::unordered_map<std::string, ByteArray> map;

        for (uint32_t i = 0; i < total_groups; i++) {
            std::string id = format("group_%u", i);
            ByteArray key;
            for (int k = 0; k < 256; k++) {
                key[k] = i; // fazer ser um numero aleatorio
            }

            map.emplace(id, key);
        }

        return map;
    }

    std::vector<NodeConfig> create_nodes() {
        std::vector<NodeConfig> nodes;

        uint32_t n = total_nodes_in_group * total_groups;

        for (uint32_t i = 0; i < n; i++) {
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
        config.alive = 500;
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
                        messages_sent_per_node
                    )
                );
            }
        }

        for (auto& worker : workers) {
            worker->start();
        }

        for (auto& worker : workers) {
            worker->wait();
        }

        return BenchmarkResult();
    }
};
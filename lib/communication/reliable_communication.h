#pragma once

#include <vector>
#include <memory>
#include <semaphore>
#include <thread>

#include "channels/channel.h"
#include "core/constants.h"
#include "utils/format.h"

class ReliableCommunication {
public:
    ReliableCommunication(std::string local_id, std::size_t _buffer_size);

    void run();

    void send(std::string id, char* m);
    std::size_t receive(char* m);

    const Node& get_node(std::string id);
    const std::vector<Node>& get_nodes();

private:
    std::unique_ptr<Channel> channel;

    char receive_buffer[INTERMEDIARY_BUFFER_SIZE]; // trocar para ser o sizeof(estrutura com metadados da msg)*100
    std::counting_semaphore<INTERMEDIARY_BUFFER_ITEMS> receive_consumer{0};
    std::counting_semaphore<INTERMEDIARY_BUFFER_ITEMS> receive_producer{INTERMEDIARY_BUFFER_ITEMS};
    int receive_buffer_start = 0;
    int receive_buffer_end = 0;

    char send_buffer[INTERMEDIARY_BUFFER_SIZE];
    std::counting_semaphore<INTERMEDIARY_BUFFER_ITEMS> send_consumer{0};
    std::counting_semaphore<INTERMEDIARY_BUFFER_ITEMS> send_producer{INTERMEDIARY_BUFFER_ITEMS};
    int send_buffer_start = 0;
    int send_buffer_end = 0;

    std::size_t buffer_size;

    std::vector<Node> nodes;

    std::vector<Node> create_nodes(std::string local_id);
};

struct ManagementThreadArgs
{
    ReliableCommunication* manager;
};

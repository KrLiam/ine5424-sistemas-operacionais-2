#pragma once

#include <vector>
#include <memory>
#include <semaphore>
#include <thread>

#include "channels/channel.h"
#include "core/buffer.h"
#include "core/constants.h"
#include "core/segment.h"
#include "core/node.h"
#include "utils/format.h"

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);

    void listen_receive();
    void listen_send();

    void send(std::string id, char *m);
    Segment receive(char *m);

    const Node &get_node(std::string id);
    const std::vector<Node> &get_nodes();

private:
    std::unique_ptr<Channel> channel;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Segment> receive_buffer{"receive"};
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Segment> send_buffer{"send"};

    std::size_t user_buffer_size;
    std::string local_id;

    std::vector<Node> nodes;

    std::vector<Node> create_nodes(std::string local_id);
};

#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <semaphore>

#include "channels/channel.h"
#include "core/buffer.h"
#include "core/constants.h"
#include "core/segment.h"
#include "core/node.h"
#include "communication/pipeline.h"
#include "communication/pipeline_step.h"
#include "utils/format.h"
#include "utils/hash.h"

class Pipeline;

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);
    ~ReliableCommunication();

    void send(std::string id, char *m);
    Message receive(char *m);

    void produce(Message message);

    const Node &get_node(std::string id);
    const Node &get_node(SocketAddress address);
    const std::map<std::string, Node> &get_nodes();
    const Node &get_local_node();

    Connection* get_connection(std::string id);

private:
    Channel* channel;

    Pipeline* pipeline;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> receive_buffer{"message receive"};
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> send_buffer{"message send"};

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> messages{"message processor"};

    std::size_t user_buffer_size;

    std::string local_id;
    std::map<std::string, Node> nodes;

    std::map<std::string, Connection*> connections;

    std::map<std::string, Node> create_nodes();

    void create_connections();
};

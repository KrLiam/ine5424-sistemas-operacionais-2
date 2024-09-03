#pragma once

#include <vector>

#include "channels/channel.h"

class ReliableCommunication {
    public:
    ReliableCommunication();
    ~ReliableCommunication();

    void initialize(std::string local_id, std::size_t _buffer_size);
    void deinitialize();

    void send(std::string id, char* m);
    std::size_t receive(char* m);

    const Node& get_node(std::string id);
    const std::vector<Node>& get_nodes();

    private:
    std::size_t buffer_size;

    Channel* channel;
    
    std::vector<Node> nodes;

    std::vector<Node> create_nodes(std::string local_id);
};

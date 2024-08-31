#pragma once

#include <vector>
#include <unordered_map>
#include <sys/socket.h>

#include "config.h"
#include "node.h"

struct Socket {

};

class Channel
{
public:
    Channel();

    ~Channel();

    void init(std::string local_id, std::vector<Node> nodes);

    void send(Node node, char *m, std::size_t size);

    std::size_t receive(char *m);

private:
    std::string local_id;
    int socket_descriptor = -1;
    std::unordered_map<std::string, Node> nodes;

    void initialize_socket_for_node(Node node);
};

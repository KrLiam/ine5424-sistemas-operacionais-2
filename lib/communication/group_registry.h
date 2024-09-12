#pragma once

#include <string>
#include <map>

#include "core/node.h"

class GroupRegistry
{
public:
    GroupRegistry(std::string local_id);
    ~GroupRegistry();

    const Node &get_node(std::string id);
    const Node &get_node(SocketAddress address);
    const std::map<std::string, Node> &get_nodes();
    const Node &get_local_node();
    Connection *get_connection(std::string id);

private:
    std::string local_id;
    std::map<std::string, Node> nodes;
    std::map<std::string, Connection *> connections;

    void read_nodes_from_configuration(std::string local_id);
    void establish_connections();
};

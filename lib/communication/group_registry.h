#pragma once

#include <string>
#include <map>

#include "core/connection.h"
#include "core/node.h"
#include "core/segment.h"

class Pipeline;

class GroupRegistry
{
public:
    GroupRegistry(std::string local_id);
    ~GroupRegistry();

    const Node &get_node(std::string id);
    const Node &get_node(SocketAddress address);
    const std::map<std::string, Node> &get_nodes();
    const Node &get_local_node();

    Connection& get_connection(SocketAddress address)
    {
        Node dest = get_node(address);
        return connections.at(dest.get_id());
    }

    bool packet_originates_from_group(Packet packet);

    void establish_connections(Pipeline& pipeline);

private:
    std::string local_id;

    std::map<std::string, Node> nodes;
    std::map<std::string, Connection> connections;

    void read_nodes_from_configuration(std::string local_id);
};

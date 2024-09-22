#pragma once

#include <string>
#include <map>

#include "communication/connection.h"
#include "core/node.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/buffer.h"

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

    bool has_connection(std::string id) {
        return connections.contains(id);
    }

    Connection& get_connection(SocketAddress address)
    {
        return get_connection(get_node(address));
    }
    Connection& get_connection(const Node& node)
    {
        return get_connection(node.get_id());
    }
    Connection& get_connection(std::string id)
    {
        return connections.at(id);
    }

    bool packet_originates_from_group(Packet packet);

    void establish_connections(
        Pipeline& pipeline,
        Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer,
        Buffer<100, std::string> &connection_update_buffer
    );

private:
    std::string local_id;

    std::map<std::string, Node> nodes;
    std::map<std::string, Connection> connections;

    void read_nodes_from_configuration(std::string local_id);
};

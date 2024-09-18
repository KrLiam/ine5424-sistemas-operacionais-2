#include "communication/group_registry.h"

GroupRegistry::GroupRegistry(std::string local_id) : local_id(local_id)
{
    read_nodes_from_configuration(local_id);
}

GroupRegistry::~GroupRegistry()
{
}

const Node &GroupRegistry::get_node(std::string id)
{
    std::map<std::string, Node>::iterator iterator = nodes.find(id);
    if (iterator != nodes.end())
    {
        return iterator->second;
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

const Node &GroupRegistry::get_node(SocketAddress address)
{
    for (auto &[id, node] : nodes)
    {
        if (address == node.get_address())
        {
            return node;
        }
    }
    throw std::invalid_argument(format("Node with address %s not found.", address.to_string().c_str()));
}

const std::map<std::string, Node> &GroupRegistry::get_nodes()
{
    return nodes;
}

const Node &GroupRegistry::get_local_node()
{
    return get_node(local_id);
}

void GroupRegistry::read_nodes_from_configuration(std::string local_id)
{
    nodes.clear();
    Config config = ConfigReader::parse_file("nodes.conf");
    for (NodeConfig node_config : config.nodes)
    {
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        nodes.emplace(node.get_id(), node);
    }
}

bool GroupRegistry::packet_originates_from_group(Packet packet)
{
    for (auto &[id, node] : nodes)
    {
        if (packet.meta.origin == node.get_address())
        {
            return true;
        }
    }
    return false;
}

void GroupRegistry::establish_connections(Pipeline &pipeline, Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer)
{
    Node local_node = get_local_node();
    for (auto &[id, node] : nodes)
        connections.emplace(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(pipeline, application_buffer, local_node, node));
}
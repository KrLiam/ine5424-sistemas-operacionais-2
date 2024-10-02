#include "core/node.h"

Node::Node(std::string id, SocketAddress address, bool remote)
    : id(id), address(address), remote(remote) {};

Node::~Node()
{
}

const std::string &Node::get_id() const
{
    return id;
}

const SocketAddress &Node::get_address() const
{
    return address;
}

bool Node::is_remote() const
{
    return remote;
}

std::string Node::to_string() const
{
    return format("%s:%s:%i", id.c_str(), address.to_string().c_str(), remote);
}

NodeMap::NodeMap() : nodes() {}

NodeMap::NodeMap(std::map<std::string, Node> nodes) : nodes(nodes) {}

const Node &NodeMap::get_node(std::string id) const
{
    auto iterator = nodes.find(id);
    if (iterator != nodes.end())
    {
        return iterator->second;
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

const Node &NodeMap::get_node(SocketAddress address) const
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

bool NodeMap::contains(SocketAddress& address) const
{
    for (auto &[id, node] : nodes)
    {
        if (address == node.get_address())
        {
            return true;
        }
    }
    return false;
}

void NodeMap::add(Node& node) {
    nodes.emplace(node.get_id(), node);
}

std::map<std::string, Node>::iterator NodeMap::begin() {
    return nodes.begin();
}

std::map<std::string, Node>::iterator NodeMap::end() {
    return nodes.end();
}

void NodeMap::clear() {
    nodes.clear();
}


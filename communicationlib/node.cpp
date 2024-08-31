#include "node.h"

Node::Node(const NodeConfig _config)
{
    config = _config;
}

Node::~Node()
{
}

void Node::set_socket(int _socket)
{
    socket = _socket;
}

void Node::set_socket_address(sockaddr_in _socket_address)
{
    socket_address = _socket_address;
}

NodeConfig Node::get_config()
{
    return config;
}

bool Node::is_remote()
{
    return remote;
}

std::string Node::to_string()
{
    return std::format("{}:{}", config.id, socket);
}
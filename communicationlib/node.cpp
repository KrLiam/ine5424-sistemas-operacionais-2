#include "node.h"

Node::Node(NodeConfig _config)
{
    config = _config;
}

Node::~Node()
{
}

int Node::get_socket() const
{
    return socket;
}

const sockaddr_in& Node::get_socket_address()
{
    return socket_address;
}

void Node::set_socket(int _socket)
{
    socket = _socket;
}

void Node::set_socket_address(sockaddr_in _socket_address)
{
    socket_address = _socket_address;
}

const NodeConfig& Node::get_config() const
{
    return config;
}

bool Node::is_remote() const
{
    return remote;
}

std::string Node::to_string() const
{
    return std::format("{}:{}", config.id, socket);
}
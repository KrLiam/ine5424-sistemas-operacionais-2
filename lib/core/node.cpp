#include "core/node.h"

Connection::Connection() : msg_num(0), state(ConnectionState::CLOSED)
{
}

const int &Connection::get_current_message_number()
{
    return msg_num;
}

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

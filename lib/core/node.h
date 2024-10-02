#pragma once

#include <condition_variable>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <map>
#include <exception>

#include "utils/config.h"
#include "utils/format.h"
#include "core/message.h"

class Node
{
private:
    std::string id;
    SocketAddress address;
    bool remote;

public:
    Node(std::string id, SocketAddress address, bool _remote);
    ~Node();

    const std::string &get_id() const;
    const SocketAddress &get_address() const;
    bool is_remote() const;

    std::string to_string() const;
};

class NodeMap {
    std::map<std::string, Node> nodes;
public:
    NodeMap();
    NodeMap(std::map<std::string, Node> nodes);

    const Node &get_node(std::string id) const;
    const Node &get_node(SocketAddress address) const;

    bool contains(SocketAddress& address) const;

    void add(Node& node);

    std::map<std::string, Node>::iterator begin();
    std::map<std::string, Node>::iterator end();

    void clear();
};

class connection_error : std::exception
{
    std::string message;
};

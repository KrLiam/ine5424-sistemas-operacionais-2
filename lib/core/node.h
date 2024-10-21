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
    bool alive;
    bool leader;

public:
    Node(std::string id, SocketAddress address, bool _remote);
    ~Node();

    const std::string &get_id() const;
    const SocketAddress &get_address() const;
    bool is_remote() const;

    bool is_alive() const
    {
        return alive;
    };
    void set_alive(bool alive)
    {
        this->alive = alive;
    }

    bool is_leader() const
    {
        return leader;
    }
    void set_leader(bool leader)
    {
        this->leader = leader;
    }

    std::string to_string() const;

    bool operator==(const Node& other) const;
};

template<> struct std::hash<Node&> {
    std::size_t operator()(const Node& node) const {
        return std::hash<SocketAddress>()(node.get_address());
    }
};

class NodeMap {
    std::map<std::string, Node> nodes;
public:
    NodeMap();
    NodeMap(std::map<std::string, Node> nodes);

    Node &get_node(std::string id);
    Node &get_node(SocketAddress address);

    Node *get_leader();

    bool contains(const SocketAddress& address) const;

    void add(Node& node);

    std::map<std::string, Node>::iterator begin();
    std::map<std::string, Node>::iterator end();
    std::map<std::string, Node>::const_iterator begin() const;
    std::map<std::string, Node>::const_iterator end() const;

    void clear();
};

class connection_error : std::exception
{
    std::string message;
};

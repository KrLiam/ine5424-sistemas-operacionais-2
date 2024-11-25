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

enum NodeState {
    NOT_INITIALIZED = 0,
    ACTIVE = 1,
    SUSPECT = 2,
    FAULTY = 3,
};

class Node
{
private:
    std::string id;
    uint32_t pid;

    SocketAddress address;
    bool remote;

    NodeState state;

    bool leader;

    // Indica se está atualmente recebendo pacotes AB. Somente para o nodo local.
    bool receiving_ab_broadcast;

public:
    Node(std::string id, SocketAddress address, NodeState state, bool remote);
    ~Node();

    const std::string &get_id() const;
    const SocketAddress &get_address() const;
    bool is_remote() const;

    bool is_alive() const
    {
        return state == ACTIVE || state == SUSPECT;
    };

    NodeState get_state() const
    {
        return state;
    }
    void set_state(NodeState state)
    {
        this->state = state;
    }

    bool is_leader() const
    {
        return leader;
    }
    void set_leader(bool leader)
    {
        this->leader = leader;
    }

    bool is_receiving_ab_broadcast() const
    {
        return receiving_ab_broadcast;
    }
    void set_receiving_ab_broadcast(bool receiving_ab_broadcast)
    {
        this->receiving_ab_broadcast = receiving_ab_broadcast;
    }

    uint32_t get_pid() const
    {
        return pid;
    }
    void set_pid(uint32_t pid)
    {
        this->pid = pid;
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
    Node &get_node(const SocketAddress& address);

    Node *get_leader();

    bool contains(const SocketAddress& address) const;

    void add(Node& node);

    std::map<std::string, Node>::iterator begin();
    std::map<std::string, Node>::iterator end();
    std::map<std::string, Node>::const_iterator begin() const;
    std::map<std::string, Node>::const_iterator end() const;

    void clear();
    std::size_t size();
};

class connection_error : std::exception
{
    std::string message;
};

#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "utils/config.h"
#include "utils/format.h"
#include "core/segment.h"

enum ConnectionState
{
    CLOSED = 0,
    AWAITING_HANDSHAKE_ACK = 1,
    ESTABLISHED = 2
};

class Connection
{
public:
    Connection();

    const int &get_current_message_number();

    int msg_num;
    ConnectionState state;
};

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

class connection_error : std::exception
{
    std::string message;
};

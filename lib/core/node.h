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
    uint32_t expected_message_number = 0;
    uint32_t next_message_number = 0;
    ConnectionState state = ConnectionState::CLOSED;

public:
    Connection();

    const uint32_t &get_next_message_number()
    {
        return next_message_number;
    }
    void increment_next_message_number()
    {
        next_message_number++;
    }

    const uint32_t &get_expected_message_number()
    {
        return expected_message_number;
    }
    void increment_expected_message_number()
    {
        expected_message_number++;
    }
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

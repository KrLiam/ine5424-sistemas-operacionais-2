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
    Connection() {}

    const int& get_msg_num()
    {
        return msg_num;
    }

    const std::vector<Packet>& get_packets_to_send()
    {
        return packets_to_send;
    }

    void forward_packet(Packet packet)
    {
        packets_to_send.push_back(packet);
    }

    int msg_num = 0;

    ConnectionState state = ConnectionState::CLOSED;
    
    std::vector<Packet> packets_to_send = std::vector<Packet>{};
    std::vector<int> packets_awaiting_ack = std::vector<int>{};

    char receive_buffer[Message::MAX_MESSAGE_SIZE];
    std::vector<int> received_fragments = std::vector<int>{};
    unsigned int last_fragment_num = INT_MAX;
    unsigned int bytes_received = 0;
};

class Node
{
    std::string id;
    SocketAddress address;
    bool remote;

public:
    Node(std::string id, SocketAddress address, bool _remote);
    ~Node();

    const std::string& get_id() const;
    const SocketAddress& get_address() const;
    bool is_remote() const;

    std::string to_string() const;
};

class connection_error : std::exception
{
    std::string message;

/*public:
    connection_error() : message("") {}
    connection_error(std::string msg) : message(msg) {}

    virtual const char *what() const throw()
{
        return message.c_str();
    }*/
};

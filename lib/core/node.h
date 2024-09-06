#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "utils/config.h"
#include "utils/format.h"

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

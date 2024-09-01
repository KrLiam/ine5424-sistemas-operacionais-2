#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "config.h"

class Node
{
    NodeConfig config;
    bool remote;

public:
    Node(const NodeConfig& _config, const bool& _remote);
    ~Node();

    const NodeConfig& get_config() const;
    const bool& is_remote() const;

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

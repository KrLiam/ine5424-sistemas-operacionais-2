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
    int socket;
    sockaddr_in socket_address;

public:
    Node(const NodeConfig _config);
    ~Node();

    const NodeConfig& get_config() const;
    bool is_remote() const;

    int get_socket() const;
    const sockaddr_in& get_socket_address();
    void set_socket(int _socket);
    void set_socket_address(sockaddr_in _socket_address);
    
    std::string to_string() const;
};

class connection_error : std::exception
{
    std::string message;

public:
    connection_error() : message("") {}
    connection_error(std::string msg) : message(msg) {}

    virtual const char *what() const throw()
{
        return message.c_str();
    }
};


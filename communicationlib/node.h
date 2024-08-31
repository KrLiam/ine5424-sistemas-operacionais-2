#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "config.h"

class Node
{
    const NodeConfig config;
    bool remote;
    int socket;
    sockaddr_in socket_address;

public:
    Node(const NodeConfig _config);
    ~Node();

    NodeConfig get_config();
    bool is_remote();

    void set_socket(int _socket);
    void set_socket_address(sockaddr_in _socket_address);
    
    std::string to_string();
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

void run_process(int node_id)
{
    Config config = ConfigReader::parse_file("nodes.conf");
    NodeConfig node_config = config.get_node(node_id);

    Node node(node_config);
}

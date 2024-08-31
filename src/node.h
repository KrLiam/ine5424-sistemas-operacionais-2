#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "config.h"


class connection_error : std::exception {
    std::string message;

 public:
    connection_error() : message("") {}
    connection_error(std::string msg) : message(msg) {}

    virtual const char* what() const throw() {
        return message.c_str();
    }
};

int create_socket(const SocketAddress& socket_address) {
    std::string ip_address = socket_address.address.to_string();
    int port = socket_address.port;

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) return -1;
    
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr_in));

    sockaddr.sin_family = AF_INET;

    int pton_result = inet_pton(AF_INET, ip_address.c_str(), &sockaddr.sin_addr);
    if (pton_result <= 0) return -1;

    sockaddr.sin_port = htons(port);

    int bind_result = bind(
        sock_fd, (const struct sockaddr*) &sockaddr, sizeof(sockaddr)
    );
    if (bind_result < 0) return -1;

    return sock_fd;
}


class Node {
    const NodeConfig config;
public:
    Node(const NodeConfig config) : config(config) {}
};


void run_process(int node_id) {
    Config config = ConfigReader::parse_file("nodes.conf");
    NodeConfig node_config = config.get_node(node_id);

    Node node(node_config);
}

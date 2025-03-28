#pragma once

#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <netinet/in.h>

#include "utils/reader.h"

class config_error : public std::runtime_error {
public:
    explicit config_error(const std::string &msg);

    virtual const char *what() const noexcept override;

    virtual std::string get_message() const;

protected:
    std::string message;
};

class port_in_use_error final : public config_error {
public:
    explicit port_in_use_error();
    explicit port_in_use_error(const std::string msg);
};

std::string read_file(const std::string &filename);

class ConfigReader;

struct IPv4
{
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;

    static IPv4 parse(std::string string);
    static IPv4 parse(ConfigReader &reader);

    std::string to_string() const;

    bool operator==(const IPv4& other) const
    {
        return other.a == a && other.b == b && other.c == c && other.d == d;
    }
};

struct SocketAddress
{
    IPv4 address;
    int port;

    std::string to_string() const;

    static SocketAddress from(sockaddr_in& address);

    bool operator==(const SocketAddress& other) const
    {
        return other.address == address && other.port == port;
    }
};

struct NodeConfig
{
    std::string id;
    SocketAddress address;

    std::string to_string() const;
};

struct Config
{
    std::vector<NodeConfig> nodes;

    NodeConfig &get_node(std::string id);

    std::string to_string() const;
};

class ConfigReader : public Reader
{
public:
    ConfigReader(std::string string);

    static ConfigReader from_file(const std::string &path);
    static Config parse_file(const std::string &path);

    IPv4 parse_ipv4();
    SocketAddress parse_socket_address();
    Config parse();
};

#pragma once

#include <arpa/inet.h>
#include <stdint.h>
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

template<> struct std::hash<IPv4> {
    std::size_t operator()(const IPv4& p) const {
        return std::hash<unsigned char>()(p.a)
            ^ std::hash<unsigned char>()(p.b)
            ^ std::hash<unsigned char>()(p.c)
            ^ std::hash<unsigned char>()(p.d);
    }
};

const IPv4 BROADCAST_ADDRESS = {255, 255, 255, 255};


struct SocketAddress
{
    IPv4 address;
    uint16_t port;

    std::string to_string() const;

    static SocketAddress from(sockaddr_in& address);

    bool operator==(const SocketAddress& other) const
    {
        return other.address == address && other.port == port;
    }
    bool operator==(const IPv4& address) const
    {
        return this->address == address;
    }
};

template<> struct std::hash<SocketAddress> {
    std::size_t operator()(const SocketAddress& p) const {
        return std::hash<IPv4>()(p.address) ^ std::hash<int>()(p.port);
    }
};


struct NodeConfig
{
    std::string id;
    SocketAddress address;

    std::string to_string() const;
};

struct IntRange {
    uint32_t min;
    uint32_t max;

    IntRange static full() {
        return IntRange(0, UINT32_MAX);
    }

    bool operator==(const IntRange& other) const;

    std::string to_string() const;

    bool contains(uint32_t value) const;
    bool contains(IntRange value) const;

    static IntRange parse(std::string string);
    static IntRange parse(Reader &reader);

    IntRange(uint32_t value);
    IntRange(uint32_t min, uint32_t max);

    uint32_t length();
};

struct FaultConfig {
    std::vector<int> faults;
    IntRange delay = {0, 0};
    double lose_chance = 0;
    double corrupt_chance = 0;
};

enum class BroadcastType {
    BEB = 0,
    URB = 1,
    AB = 2
};

struct Config
{
    std::vector<NodeConfig> nodes;
    unsigned int alive = 1000;
    BroadcastType broadcast = BroadcastType::BEB;
    FaultConfig faults;

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
    std::vector<NodeConfig> parse_nodes();
    uint32_t parse_alive();
    BroadcastType parse_broadcast();
    FaultConfig parse_faults();
    Config parse();
};

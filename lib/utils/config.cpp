
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/format.h"


config_error::config_error(const std::string &msg)
    : std::runtime_error(msg), message(msg) {}

const char* config_error::what() const noexcept {
    return message.c_str();
}

std::string config_error::get_message() const {
    return message;
}


port_in_use_error::port_in_use_error()
        : config_error("Port is already in use.") {}
port_in_use_error::port_in_use_error(const std::string msg)
        : config_error(msg) {}


IPv4 IPv4::parse(std::string string) {
    ConfigReader reader(string);
    return IPv4::parse(reader);
}
IPv4 IPv4::parse(ConfigReader& reader) {
    int a = reader.read_int();
    reader.expect('.');
    int b = reader.read_int();
    reader.expect('.');
    int c = reader.read_int();
    reader.expect('.');
    int d = reader.read_int();

    return IPv4(a, b, c, d);
}
std::string IPv4::to_string() const
{
    return format("%i.%i.%i.%i", a, b, c, d);
}

std::string SocketAddress::to_string() const
{
    return format("%s:%i", address.to_string().c_str(), port);
}

SocketAddress SocketAddress::from(sockaddr_in& address)
{
    char remote_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, remote_address, INET_ADDRSTRLEN);
    int remote_port = ntohs(address.sin_port);
    return SocketAddress{IPv4::parse(remote_address), remote_port};
}

std::string NodeConfig::to_string() const
{
    return format("{%s, %s}", id, address.to_string().c_str());
}

NodeConfig &Config::get_node(std::string id)
{
    for (NodeConfig &node : nodes)
    {
        if (node.id == id)
            return node;
    }
    throw std::invalid_argument(
        format("Node id %s not found in config.", id.c_str()));
}

std::string Config::to_string() const
{
    std::string acc;

    acc += "nodes = {\n";
    for (const NodeConfig &node : nodes)
    {
        acc += format("    %s,\n", node.to_string().c_str());
    }
    acc += "};";

    return acc;
}

ConfigReader::ConfigReader(std::string string) : Reader(string) {}

ConfigReader ConfigReader::from_file(const std::string &path)
{
    return ConfigReader(read_file(path));
}

Config ConfigReader::parse_file(const std::string &path)
{
    ConfigReader reader = ConfigReader::from_file(path);
    return reader.parse();
}

SocketAddress ConfigReader::parse_socket_address()
{
    IPv4 ip = IPv4::parse(*this);
    expect(':');
    int port = read_int();

    return SocketAddress{ip, port};
}

std::vector<NodeConfig> ConfigReader::parse_nodes()
{
    std::vector<NodeConfig> nodes;

    expect('{');

    while (true)
    {
        char ch = peek();

        if (ch == '}')
            break;

        expect('{');

        std::string id = read_word();
        expect(',');
        SocketAddress address = parse_socket_address();

        NodeConfig node{id, address};
        nodes.push_back(node);

        expect('}');
        read(',');
    }

    expect('}');

    return nodes;
}

uint32_t ConfigReader::parse_alive()
{
    return read_int();
}

BroadcastType ConfigReader::parse_broadcast()
{
    std::string value = read_word();
    // TODO seria bom converter pra lowercase

    if (value == "beb") return BroadcastType::BEB;
    if (value == "urb") return BroadcastType::URB;
    if (value == "ab") return BroadcastType::AB;

    throw parse_error(format("Invalid broadcast type '%s' at config file.", value.c_str()));
}

Config ConfigReader::parse()
{
    reset();

    Config config;

    while (peek()) {
        int pos = get_pos();
        std::string target = read_word();

        if (!target.length()) {
            throw parse_error(
                format("Unexpected '%c' at pos %i of config file.", peek(), get_pos())
            );
        }

        expect('=');

        if (target == "nodes") {
            config.nodes = parse_nodes();
        }
        else if (target == "alive") {
            config.alive = parse_alive();
        }
        else if (target == "broadcast") {
            config.broadcast = parse_broadcast();
        }
        else throw parse_error(
            format("Invalid '%s' at pos %i of config file.", target.c_str(), pos)
        );

        expect(';');
    }

    return config;
}

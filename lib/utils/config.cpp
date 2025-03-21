
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

Config ConfigReader::parse()
{
    reset();

    std::vector<NodeConfig> nodes;

    expect("nodes");
    expect('=');
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
    expect(';');

    return Config{nodes};
}

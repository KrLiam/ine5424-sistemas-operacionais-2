
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
IPv4 IPv4::parse(Reader& reader) {
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
    return SocketAddress{IPv4::parse(remote_address), (uint16_t)remote_port};
}

std::string NodeConfig::to_string() const
{
    return format("{%s, %s}", id, address.to_string().c_str());
}

IntRange IntRange::parse(std::string string) {
    Reader reader(string);
    return IntRange::parse(reader);
}
IntRange IntRange::parse(Reader &reader) {
    if (reader.read('*')) return IntRange::full();

    int value = reader.read_int();

    Override ovr = reader.override_whitespace(false);

    char ch = reader.peek();
    if (ch != '.') return IntRange(value, value);

    reader.expect("..");

    int end = reader.read_int();

    return IntRange(value, end);
}

IntRange::IntRange(uint32_t value) : IntRange(value, value) {}
IntRange::IntRange(uint32_t min, uint32_t max) :
    min(std::min(min, max)), max(std::max(min, max)) {}

bool IntRange::operator==(const IntRange& other) const {
    return min == other.min && max == other.max;
}

std::string IntRange::to_string() const {
    if (min == max) return std::to_string(min);
    if (min == 0 && max == UINT32_MAX) return "*";
    return format("%i..%i", min, max);
}

bool IntRange::contains(IntRange value) const {
    return min <= value.min && value.max <= max;
}
bool IntRange::contains(uint32_t value) const {
    return min <= value && value <= max;
}

uint32_t IntRange::length() {
    return max - min + 1;
}

const NodeConfig &Config::get_node(std::string id) const
{
    for (const NodeConfig &node : nodes)
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

    return SocketAddress{ip, (uint16_t)port};
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
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "beb") return BroadcastType::BEB;
    if (value == "urb") return BroadcastType::URB;
    if (value == "ab") return BroadcastType::AB;

    throw parse_error(format("Invalid broadcast type '%s' at config file.", value.c_str()));
}

FaultConfig ConfigReader::parse_faults() {
    FaultConfig config;

    expect('{');

    while (true) {
        char ch = peek();
        if (!ch || ch == '}') break;

        expect('{');

        std::string key = read_word();

        expect('=');

        if (key == "corrupt") {
            config.corrupt_chance = static_cast<double>(read_int()) / 100.0;
        }
        else if (key == "drop") {
            config.lose_chance = static_cast<double>(read_int()) / 100.0;
        }
        else if (key == "delay") {
            config.delay = IntRange::parse(*this);
            Config::ACK_TIMEOUT = (int)config.delay.max * 2 + 500;
        }
        else throw parse_error(format("Unknown key '%s' in faults.", key.c_str()));

        expect('}');

        if (!read(',')) break;
    }

    expect('}');

    return config;
}

std::unordered_map<std::string, ByteArray> ConfigReader::parse_groups() {
    expect('{');

    std::unordered_map<std::string, ByteArray> groups;

    while (!eof()) {
        char ch = peek();
        if (!ch || ch == '}') break;

        std::string id = read_word();
        expect('=');
        ByteArray key = Aes256::parse_hex_key(*this);

        if (groups.contains(id)) throw parse_error(format("Duplicated group entry of id '%s' in config file.", id.c_str()));
        groups.emplace(id, key);

        if (!read(',')) break;
    }

    expect('}');

    return groups;
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
        else if (target == "faults") {
            config.faults = parse_faults();
        }
        else if (target == "groups") {
            config.groups = parse_groups();
        }
        else throw parse_error(
            format("Invalid '%s' at pos %i of config file.", target.c_str(), pos)
        );

        expect(';');
    }

    return config;
}

std::string SocketAddress::serialize() const
{
    std::string serialized;
    serialized += address.a;
    serialized += address.b;
    serialized += address.c;
    serialized += address.d;
    serialized += (port) & 0xff;
    serialized += (port >> 8) & 0xff;
    return serialized;
}

int Config::ACK_TIMEOUT = 100;

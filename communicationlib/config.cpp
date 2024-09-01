
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <format>

#include "config.h"
#include "log.h"


parse_error::parse_error() : message("") {}
parse_error::parse_error(std::string msg) : message(msg) {}

const char* parse_error::what() const throw() {
    return message.c_str();
}


std::string read_file(const std::string& filename) {    
    std::ifstream f(filename);

    if (!f.good()) throw std::invalid_argument(
        std::format("File '{}' does not exist.", filename)
    );

    f.seekg(0, std::ios::end);
    size_t size = f.tellg();

    std::string buffer(size, ' ');

    f.seekg(0);
    f.read(&buffer[0], size);

    return buffer;
}



std::string IPv4::to_string() const {
    return std::format("{}.{}.{}.{}", a, b, c, d);
}

std::string SocketAddress::to_string() const {
    return std::format("{}:{}", address.to_string(), port);
}

std::string NodeConfig::to_string() const {
    return std::format("{{{}, {}}}", id, address.to_string());
}


NodeConfig& Config::get_node(std::string id) {
    for (NodeConfig& node : nodes) {
        if (node.id == id) return node;
    }
    throw std::invalid_argument(
        std::format("Node id {} not found in config.", id)
    );
}

std::string Config::to_string() const {
    std::string acc;

    acc += "nodes = {\n";
    for (const NodeConfig& node : nodes) {
        acc += std::format("    {},\n", node.to_string());
    }
    acc += "};";

    return acc;
}

template <typename T>
Override<T>::Override(T* ref, T value) : ref(ref), value(value) {
    previous_value = *ref;
    *ref = value;
}

template <typename T>
Override<T>::~Override() {
    *ref = previous_value;
}


ConfigReader::ConfigReader(std::string string) : str(string) {}

ConfigReader ConfigReader::from_file(const std::string& path) {
    return ConfigReader(read_file(path));
}

Config ConfigReader::parse_file(const std::string& path) {
    ConfigReader reader = ConfigReader::from_file(path);
    return reader.parse();
}

int ConfigReader::size() {
    return str.size();
}

bool ConfigReader::eof() {
    return pos >= size();
}

void ConfigReader::advance(int amount = 1) {
    pos = std::min(size(), pos + amount);
}

char ConfigReader::peek() {
    if (eof()) return 0;

    char ch = str.at(pos);

    if (ignores_whitespace && isspace(ch)) {
        advance();
        return peek();
    }

    return ch;
}

char ConfigReader::read(char ch = 0) {
    char p = peek();

    if (!p) return 0;
    
    if (ch == 0 || ch == p) {
        advance();
        return p;
    }

    return 0;
}

int ConfigReader::read_int() {
    char ch = peek();
    int start = pos;
    
    Override ovr(&ignores_whitespace, false);

    while(ch && isdigit(ch)) {
        advance();
        ch = peek();
    }

    std::string string = str.substr(start, pos - start);

    if (!string.size()) return 0;
    return std::stoi(string);
}

std::string ConfigReader::read_word() {
    char ch = peek();
    int start = pos;
    
    Override ovr(&ignores_whitespace, false);

    while(ch && (isdigit(ch) || isalpha(ch)) ) {
        advance();
        ch = peek();
    }

    return str.substr(start, pos - start);
}

void ConfigReader::consume_space() {
    while (peek() && isspace(peek())) {
        advance();
    }
}

char ConfigReader::expect(char ch) {
    char result = read(ch);
    if (!result) {
        throw parse_error(std::format("Expected character '{}' at position {}. Got '{}'.", ch, std::to_string(pos), peek()));
    }
    return result;
}
void ConfigReader::expect(const std::string& chars) {
    Override ovr(&ignores_whitespace, false);

    for (char ch : chars) {
        expect(ch);
    }
}

IPv4 ConfigReader::parse_ipv4() {
    int a = read_int();
    expect('.');
    int b = read_int();
    expect('.');
    int c = read_int();
    expect('.');
    int d = read_int();

    return IPv4(a, b, c, d);
}

SocketAddress ConfigReader::parse_socket_address() {
    IPv4 ip = parse_ipv4();
    expect(':');
    int port = read_int();

    return SocketAddress{ip, port};
}

Config ConfigReader::parse() {
    pos = 0;

    std::vector<NodeConfig> nodes;

    expect("nodes");
    expect('=');
    expect('{');

    while (true) {
        char ch = peek();

        if (ch == '}') break;

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




#pragma once

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>


class parse_error : std::exception {
    std::string message;

 public:
    parse_error();
    parse_error(std::string msg);

    virtual const char* what() const throw();
};


std::string read_file(const std::string& filename);


struct IPv4 {
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;

    std::string to_string() const;
};

struct SocketAddress {
    IPv4 address;
    int port;

    std::string to_string() const;
};

struct NodeConfig {
    std::string id;
    SocketAddress address;

    std::string to_string() const;
};

struct Config {
    std::vector<NodeConfig> nodes;

    NodeConfig& get_node(std::string id);

    std::string to_string() const;
};


template <typename T>
class Override {
    T* ref;
    T value;
    T previous_value;

public:
    Override(T* ref, T value);
    ~Override();
};

class ConfigReader {
    std::string str;
    int pos = 0;

    bool ignores_whitespace = true;

public:
    ConfigReader(std::string string);

    static ConfigReader from_file(const std::string& path);
    static Config parse_file(const std::string& path);

    int size();
    bool eof();

    void advance(int amount);
    char peek();
    char read(char ch);
    int read_int();
    std::string read_word();

    void consume_space();

    char expect(char ch);
    void expect(const std::string& chars);

    IPv4 parse_ipv4();
    SocketAddress parse_socket_address();
    Config parse();
};



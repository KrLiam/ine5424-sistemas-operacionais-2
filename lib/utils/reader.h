#pragma once

#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <netinet/in.h>


class parse_error final : public std::runtime_error {
public:
    explicit parse_error();
    explicit parse_error(const std::string &msg);
};

std::string read_file(const std::string &filename);

class ConfigReader;

template <typename T>
class Override
{
    T *ref;
    T value;
    T previous_value;

public:
    Override(T *ref, T value);
    ~Override();
};

class Reader
{
    std::string str;
    int pos = 0;

    bool ignores_whitespace = true;

public:
    Reader(std::string string);

    static Reader from_file(const std::string &path);

    int size();
    bool eof();

    Override<bool> override_whitespace(bool value);

    void reset();
    void advance(int amount);
    char peek();
    char read(char ch);
    int read_int();
    std::string read_word();

    void consume_space();

    char expect(char ch);
    void expect(const std::string &chars);
};

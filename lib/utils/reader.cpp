
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>

#include "utils/reader.h"
#include "utils/log.h"
#include "utils/format.h"


parse_error::parse_error()
        : std::runtime_error("Parse error occurred.") {}
parse_error::parse_error(const std::string& msg)
        : std::runtime_error(msg) {}


std::string read_file(const std::string &filename)
{
    std::ifstream f(filename);

    if (!f.good())
        throw std::invalid_argument(
            format("File '%s' does not exist.", filename.c_str()));

    f.seekg(0, std::ios::end);
    size_t size = f.tellg();

    std::string buffer(size, ' ');

    f.seekg(0);
    f.read(&buffer[0], size);

    return buffer;
}

template <typename T>
Override<T>::Override(T *ref, T value) : ref(ref), value(value)
{
    previous_value = *ref;
    *ref = value;
}

template <typename T>
Override<T>::~Override()
{
    *ref = previous_value;
}

Reader::Reader(std::string string) : str(string) {}

Reader Reader::from_file(const std::string &path)
{
    return Reader(read_file(path));
}

int Reader::get_pos()
{
    return pos;
}

int Reader::size()
{
    return str.size();
}

bool Reader::eof()
{
    return pos >= size();
}

void Reader::reset()
{
    pos = 0;
}

Override<bool> Reader::override_whitespace(bool value)
{
    return Override(&ignores_whitespace, value);
}

void Reader::advance()
{
    advance(1);
}
void Reader::advance(int amount)
{
    pos = std::min(size(), pos + amount);
}

char Reader::peek()
{
    if (eof())
        return 0;

    char ch = str.at(pos);

    if (ignores_whitespace && isspace(ch))
    {
        advance();
        return peek();
    }

    return ch;
}

char Reader::read(char ch = 0)
{
    char p = peek();

    if (!p)
        return 0;

    if (ch == 0 || ch == p)
    {
        advance();
        return p;
    }

    return 0;
}

int Reader::read_int()
{
    char ch = peek();
    int start = pos;

    Override ovr = override_whitespace(false);

    while (ch && isdigit(ch))
    {
        advance();
        ch = peek();
    }

    std::string string = str.substr(start, pos - start);

    if (!string.size())
        return 0;
    return std::stoi(string);
}

std::string Reader::read_word()
{
    char ch = peek();
    int start = pos;
    
    Override ovr = override_whitespace(false);

    while (ch && (isdigit(ch) || isalpha(ch)))
    {
        advance();
        ch = peek();
    }

    return str.substr(start, pos - start);
}

void Reader::consume_space()
{
    while (peek() && isspace(peek()))
    {
        advance();
    }
}

char Reader::expect(char ch)
{
    char result = read(ch);
    if (result != ch)
    {
        char p = peek();
        std::string got = p ? std::string(1, p) : "eof";
        throw parse_error("Expected character '" + std::string(1, ch) +
                          "' at position " + std::to_string(pos) +
                          ". Got '" + got + "'.");
    }
    return result;
}

void Reader::expect(const std::string &chars)
{
    peek();
    Override ovr = override_whitespace(false);

    for (char ch : chars)
    {
        expect(ch);
    }
}

#include "arguments.h"

std::vector<int> parse_fault_list(Reader& reader) {
    std::vector<int> values;

    reader.expect('[');

    while (!reader.eof()) {
        char ch = reader.peek();

        if (ch == ']') break;

        if (ch == 'l' || ch == 'L') {
            reader.advance();
            values.push_back(INT_MAX);
        }
        else if (isdigit(ch)) {
            int value = reader.read_int();
            values.push_back(value);
        }
        else throw std::invalid_argument(
            format("Invalid character '%c' at pos %i", ch, reader.get_pos())
        );

        if (!reader.read(',')) break;
    }

    reader.expect(']');

    return values;
}

Arguments parse_arguments(int argc, char* argv[]) {
    std::vector<int> faults;
    std::vector<std::shared_ptr<Command>> send_commands;

    std::string value;
    for (int i=1; i < argc; i++) {
        value += argv[i];
        value += " ";
    }

    Reader reader(value);

    std::string node_id = reader.read_word();

    if (!node_id.length()) throw std::invalid_argument(
        format("Missing node id argument. Usage:\n%s <id-int>", argv[0])
    );

    while (!reader.eof()) {
        char ch = reader.peek();
        if (!ch) break;

        std::string flag;

        if (ch == '-') {
            reader.advance();
            Override ovr = reader.override_whitespace(false);
            flag = reader.read_word();
        }
        else {
            throw std::invalid_argument(
                format("Invalid character '%c at pos %i", ch, reader.get_pos())
            );  
        }

        if (!flag.length()) throw std::invalid_argument(
            format("Missing flag name after - at pos %s", reader.get_pos())
        );

        if (flag == "f") {
            faults = parse_fault_list(reader);
        }
        else if (flag == "s") {
            send_commands = parse_commands(reader);
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return Arguments{node_id, faults, send_commands};
}

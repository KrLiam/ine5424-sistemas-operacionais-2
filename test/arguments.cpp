#include "arguments.h"

Arguments parse_arguments(int argc, char* argv[]) {
    Arguments args;

    std::string value;
    for (int i=1; i < argc; i++) {
        value += argv[i];
        value += " ";
    }

    Reader reader(value);

    if (reader.read("test")) {
        args.test = true;
        args.case_path = parse_path(reader);
        if (args.case_path.empty()) throw std::invalid_argument(
            format("Missing case file path. Usage:\n%s test <case>", argv[0])
        );
    }
    else {
        args.node_id = reader.read_word();

        if (!args.node_id.length()) throw std::invalid_argument(
            format("Missing node id argument. Usage:\n%s <id-int>", argv[0])
        );
    }

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

        if (flag == "s") {
            args.send_commands = parse_commands(reader);
        }
        else if (flag == "log-trail") {
            args.log_trail = reader.read_int();
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return args;
}

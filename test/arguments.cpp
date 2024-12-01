#include "arguments.h"

Arguments parse_arguments(int argc, char* argv[]) {
    Arguments args;
    
    args.program_name = argv[0];

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
    else if (reader.peek() != '-') {
        args.node_id = reader.read_word();

        if (!args.node_id.length()) throw std::invalid_argument(
            format("Missing node id argument. Usage:\n%s <id-int>", argv[0])
        );
    }

    while (!reader.eof()) {
        char ch = reader.peek();
        if (!ch) break;

        std::string flag;

        bool long_arg = false;
        if (ch == '-') {
            reader.advance();
            Override ovr = reader.override_whitespace(false);
            long_arg = reader.read('-');
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

        if ((!long_arg && flag == "s") || (long_arg && flag=="send")) {
            args.send_commands = parse_commands(reader);
        }
        else if ((!long_arg && flag=="a") || (long_arg && flag=="address")) {
            args.address = SocketAddress::parse(reader);
        }
        else if ((!long_arg && flag=="v") || (long_arg && flag=="verbose")) {
            args.verbose = isdigit(reader.peek()) ? reader.read_int() : true;
        }
        else if ((!long_arg && flag=="t") || (long_arg && flag=="log-tail")) {
            args.specified_log_tail = true;
            args.log_tail = isdigit(reader.peek()) ? reader.read_int() : true;
        }
        else if ((!long_arg && flag=="l") || (long_arg && flag == "log-level")) {
            args.log_level = LogLevel::parse(reader);
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return args;
}

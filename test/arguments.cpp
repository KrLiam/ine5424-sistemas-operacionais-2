#include "arguments.h"

BenchmarkMode parse_benchmark_mode(Reader& reader) {
    std::string value = reader.read_word();
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "send") return BenchmarkMode::SEND;
    if (value == "beb") return BenchmarkMode::BEB;
    if (value == "urb") return BenchmarkMode::URB;
    if (value == "ab") return BenchmarkMode::AB;

    throw parse_error(format("Invalid benchmark mode '%s'.", value.c_str()));
}

uint32_t parse_byte_unit(Reader& reader) {
    std::string value = reader.read_word();
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "b") return 1;
    if (value == "kb") return 1000;
    if (value == "kib") return 1024;
    if (value == "mb") return 1000*1000;
    if (value == "mib") return 1024*1024;
    if (value == "gb") return 1000*1000*1000;
    if (value == "gib") return 1024*1024*1024;

    if (value.length()) parse_error(
        format("Invalid byte unit '%s'. Valid values are B, KB, KiB, MB, MiB, GB or GiB.", value.c_str())
    );

    return 1;
}

const char* benchmark_mode_to_string(BenchmarkMode mode) {
    if (mode == BenchmarkMode::SEND) return "SEND";
    if (mode == BenchmarkMode::URB) return "BEB";
    if (mode == BenchmarkMode::AB) return "AB";
    return "BEB";
}

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
    else if (reader.read("benchmark")) {
        args.benchmark = true;
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
        else if ((!long_arg && flag=="r") || (long_arg && flag == "read")) {
            args.max_read_operations = reader.read_int();
        }
        else if ((!long_arg && flag=="w") || (long_arg && flag == "write")) {
            args.max_write_operations = reader.read_int();
        }
        else if (long_arg && flag == "out-file") {
            args.out_file = reader.peek() == '-' ? "" : parse_path(reader);
        }
        else if (long_arg && flag == "num-groups") {
            args.num_groups = reader.read_int();
        }
        else if (long_arg && flag == "num-nodes") {
            args.num_nodes = reader.read_int();
        }
        else if (long_arg && flag == "bytes-per-node") {
            args.bytes_per_node = reader.read_double() * parse_byte_unit(reader);
        }
        else if (long_arg && flag == "node-interval") {
            args.node_interval = IntRange::parse(reader);
        }
        else if (long_arg && flag == "max-message-size") {
            args.max_message_size = reader.read_int();
        }
        else if (long_arg && flag == "mode") {
            args.mode = parse_benchmark_mode(reader);
        }
        else if (long_arg && flag == "no-encryption") {
            args.no_encryption = true;
        }
        else if (long_arg && flag == "no-checksum") {
            args.no_checksum = true;
        }
        else {
            throw std::invalid_argument(
                format("Unknown flag '%s' at pos %i", flag.c_str(), reader.get_pos() - flag.length())
            );
        }
    }

    return args;
}

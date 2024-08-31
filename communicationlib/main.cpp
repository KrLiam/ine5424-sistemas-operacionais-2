#include <iostream>
#include <vector>
#include <format>

#include "node.h"


struct Arguments {
    std::string node_id;
};

Arguments parse_arguments(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::invalid_argument(
            std::format("Missing node id argument. Usage:\n{} <id-int>", argv[0])
        );
    }

    ConfigReader reader(argv[1]);
    std::string node_id = reader.read_word();

    if (!reader.eof()) {
        throw std::invalid_argument(
            std::format("Node id must be alphanumeric.", argv[1])
        );
    }

    return Arguments{node_id};
}


void run_process(std::string node_id)
{
    Config config = ConfigReader::parse_file("nodes.conf");
    NodeConfig node_config = config.get_node(node_id);

    Node node(node_config);
}

int main(int argc, char* argv[]) {
    try {
        Arguments args = parse_arguments(argc, argv);
        run_process(args.node_id);
    }
    catch (std::exception& error) {
        std::cout << "Error: " << error.what() << std::endl;
    }
}

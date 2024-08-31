#include <iostream>
#include <vector>
#include <format>

#include "node.h"


struct Arguments {
    int node_id;
};

Arguments parse_arguments(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::invalid_argument(
            std::format("Missing node id argument. Usage:\n{} <id-int>", argv[0])
        );
    }

    int node_id;

    try {
        node_id = std::stoi(argv[1]);
    }
    catch (std::invalid_argument& error) {
        throw std::invalid_argument(
            std::format("Invalid node id argument '{}'. Must be an int", argv[1])
        );
    }

    return Arguments{node_id};
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

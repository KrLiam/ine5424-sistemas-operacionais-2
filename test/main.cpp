// main.cpp
#include "process_runner.h"
#include "utils/log.h"

int main(int argc, char* argv[]) {
    try {
        Arguments args = parse_arguments(argc, argv);
        run_process(args.node_id);
    }
    catch (const std::exception& error) {
        log_error(error.what());
    }
}

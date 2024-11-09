#include "utils/log.h"

#include "arguments.h"
#include "process_runner.h"


int main(int argc, char* argv[]) {
    try {
        Arguments args = parse_arguments(argc, argv);
        Runner runner(args);
        runner.run();
    }
    catch (const std::exception& error) {
        log_error(error.what());
    }
}

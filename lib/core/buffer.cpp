
#include "core/buffer.h"

buffer_termination::buffer_termination()
        : std::runtime_error("Buffer was terminated.") {}
buffer_termination::buffer_termination(const std::string& msg)
        : std::runtime_error(msg) {}

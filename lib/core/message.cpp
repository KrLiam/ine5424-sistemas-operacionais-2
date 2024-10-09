
#include "core/message.h"

bool message_type::is_application(MessageType type) {
    return static_cast<int>(type) & 0b01;
}
bool message_type::is_broadcast(MessageType type) {
    return static_cast<int>(type) & 0b10;
}

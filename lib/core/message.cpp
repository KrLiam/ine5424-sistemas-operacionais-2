
#include "core/message.h"

MessageType message_type::from_broadcast_type(BroadcastType type) {
    switch (type) {
        case BroadcastType::BEB: return MessageType::BEB;
        case BroadcastType::URB: return MessageType::URB;
        case BroadcastType::AB: return MessageType::AB_REQUEST;
    }
    return MessageType::BEB;
}

bool message_type::is_application(MessageType type) {
    return static_cast<int>(type) & 0b001;
}
bool message_type::is_data(MessageType type) {
    return static_cast<int>(type) & 0b010;
}
bool message_type::is_broadcast(MessageType type) {
    return static_cast<int>(type) & 0b100;
}
bool message_type::is_urb(MessageType type) {
    return type == MessageType::URB || type == MessageType::AB_URB;
}
bool message_type::is_atomic(MessageType type) {
    return type == MessageType::AB_URB;
}

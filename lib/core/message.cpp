
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
bool message_type::is_heartbeat(MessageType type) {
    return type == MessageType::HEARTBEAT;
}
const char* message_type::to_string(MessageType type) {
    if (message_type::message_type_name.contains(type))
        return message_type::message_type_name.at(type);
    return "UNKNOWN";
}

MessageSequenceType MessageIdentity::sequence_type() const {
    if (message_type::is_heartbeat(msg_type)) return MessageSequenceType::HEARTBEAT;
    if (message_type::is_atomic(msg_type)) return MessageSequenceType::ATOMIC;
    if (message_type::is_broadcast(msg_type)) return MessageSequenceType::BROADCAST;
    return MessageSequenceType::UNICAST;
}
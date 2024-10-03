#include "core/packet.h"

bool MessageIdentity::operator==(const MessageIdentity& other) const {
    return origin == other.origin
        && msg_num == other.msg_num
        && sequence_type == other.sequence_type;
}
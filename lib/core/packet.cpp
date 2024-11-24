#include "core/packet.h"

bool MessageIdentity::operator==(const MessageIdentity& other) const {
    return origin == other.origin
        && msg_num == other.msg_num
        && msg_type == other.msg_type;
}

Packet create_ack(const Packet& packet)
{
    return create_ack(packet, packet.meta.origin);
}
Packet create_ack(const Packet& packet, SocketAddress destination)
{
    PacketData data;
    memset(&data, 0, sizeof(PacketData));

    data.header = {
        id : packet.data.header.id,
        fragment_num : packet.data.header.fragment_num,
        checksum : 0,
        flags : ACK,
        process_uuid: {0}
    };
    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        origin : packet.meta.destination,
        destination : destination,
        message_length : 0,
        expects_ack : 0
    };

    return {
        data : data,
        meta : meta
    };
}
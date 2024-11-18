#include <cmath>

#include "pipeline/fragmentation/fragmenter.h"


Fragmenter::Fragmenter(const Message& message) : message(message) {
    total_fragments = ceil((double) message.length / PacketData::MAX_MESSAGE_SIZE);
}

Packet Fragmenter::create_packet() {
    if (!has_next()) return Packet{};

    bool last_fragment = i == total_fragments - 1;
    
    int message_length = last_fragment ?
        message.length - i * PacketData::MAX_MESSAGE_SIZE :
        PacketData::MAX_MESSAGE_SIZE;

    PacketMetadata meta = {
        transmission_uuid : message.transmission_uuid,
        origin : message.origin,
        destination : message.destination,
        message_length : message_length,
        expects_ack : 1
    };

    PacketData data = {
        header : {
            id : message.id,
            fragment_num : i,
            checksum : 0,
            flags : static_cast<uint8_t>(last_fragment ? END : 0),
            uuid: {0}
        },
        message_data : 0
    };
    memcpy(data.message_data, &message.data[i * PacketData::MAX_MESSAGE_SIZE], meta.message_length);

    return Packet{
        data: data,
        meta: meta
    };
}

unsigned int Fragmenter::get_total_fragments() {
    return total_fragments;
}

bool Fragmenter::has_next() {
    return i < total_fragments;
}

bool Fragmenter::next(Packet* packet) {
    if (!has_next()) return false;

    *packet = create_packet();
    i++;

    return true;
}
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
        origin : message.origin,
        destination : message.destination,
        time : 0,
        message_length : message_length,
        expects_ack : 1
    };

    PacketData data = {
        header : {
            msg_num : message.number,
            fragment_num : i,
            checksum : 0,
            window : 0,
            ack : 0,
            rst : 0,
            syn : 0,
            fin : 0,
            extra : 0,
            end : last_fragment,
            type : message.type,
            reserved : 0
        },
        message_data : 0
    };
    strncpy(data.message_data, &message.data[i * PacketData::MAX_MESSAGE_SIZE], meta.message_length);

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
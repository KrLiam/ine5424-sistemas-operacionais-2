#pragma once

#include <vector>

#include "core/segment.h"
#include "utils/log.h"

class FragmentAssembler
{
private:
    unsigned int bytes_received = 0;
    uint32_t last_fragment_number = INT_MAX;
    std::vector<uint32_t> received_fragments = std::vector<uint32_t>{};
    Message message;

public:
    FragmentAssembler() {}

    bool has_received(Packet packet)
    {
        uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
        return std::find(received_fragments.begin(), received_fragments.end(), fragment_number) != received_fragments.end();
    }

    bool has_received_all_packets()
    {
        return last_fragment_number == received_fragments.size() - 1;
    }

    void add_packet(Packet packet)
    {
        uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
        received_fragments.push_back(fragment_number);

        unsigned int pos_in_msg = packet.data.header.fragment_num * PacketData::MAX_MESSAGE_SIZE;
        unsigned int len = packet.meta.message_length;
        strncpy(&message.data[pos_in_msg], packet.data.message_data, len);
        bytes_received += len;

        message.origin = packet.meta.origin;
        message.destination = packet.meta.destination;

        bool more_fragments = packet.data.header.more_fragments;
        if (!more_fragments)
        {
            log_debug("Packet ", packet.to_string(), " is the last one of its message.");
            last_fragment_number = fragment_number;
        }
    }

    Message& assemble()
    {
        message.length = bytes_received;
        return message;
    }
};

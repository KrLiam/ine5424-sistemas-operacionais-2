#include "fragment_assembler.h"

FragmentAssembler::FragmentAssembler() : bytes_received(0), last_fragment_number(INT_MAX), received_fragments()
{
}

FragmentAssembler::~FragmentAssembler()
{
}

bool FragmentAssembler::has_received(Packet packet)
{
    uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
    return std::find(received_fragments.begin(), received_fragments.end(), fragment_number) != received_fragments.end();
}

bool FragmentAssembler::has_received_all_packets()
{
    return last_fragment_number == received_fragments.size() - 1;
}

void FragmentAssembler::add_packet(Packet packet)
{
    uint32_t fragment_number = (uint32_t)packet.data.header.fragment_num;
    received_fragments.push_back(fragment_number);

    unsigned int pos_in_msg = packet.data.header.fragment_num * PacketData::MAX_MESSAGE_SIZE;
    unsigned int len = packet.meta.message_length;
    strncpy(&message.data[pos_in_msg], packet.data.message_data, len);
    bytes_received += len;

    message.number = packet.data.header.msg_num;
    message.type = static_cast<MessageType>(packet.data.header.type);
    message.origin = packet.meta.origin;
    message.destination = packet.meta.destination;

    bool more_fragments = packet.data.header.more_fragments;
    if (!more_fragments)
    {
        log_debug("Packet ", packet.to_string(), " is the last one of its message.");
        last_fragment_number = fragment_number;
    }
}

Message &FragmentAssembler::assemble()
{
    message.length = bytes_received;
    return message;
}
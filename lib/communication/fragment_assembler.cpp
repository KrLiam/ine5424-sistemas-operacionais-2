#include "fragment_assembler.h"

FragmentAssembler::FragmentAssembler() : bytes_received(0), last_fragment_number(INT_MAX), received_fragments()
{
}

FragmentAssembler::~FragmentAssembler()
{
}

bool FragmentAssembler::has_received(const Packet& packet)
{
    uint32_t fragment_number = packet.data.header.fragment_num;
    return received_fragments.contains(fragment_number);
}

bool FragmentAssembler::is_complete()
{
    return last_fragment_number == received_fragments.size() - 1;
}

bool FragmentAssembler::add_packet(const Packet& packet)
{
    if (has_received(packet))
    {
        log_debug("Ignoring duplicated packet ", packet.to_string(), ".");
        return false;
    };

    if (packet.data.header.type != MessageType::DATA)
    {
        return false;
    }

    const PacketHeader& header = packet.data.header;

    uint32_t fragment_number = header.fragment_num;
    bool more_fragments = header.more_fragments;

    received_fragments.emplace(fragment_number);

    unsigned int pos_in_msg = header.fragment_num * PacketData::MAX_MESSAGE_SIZE;
    unsigned int len = packet.meta.message_length;
    strncpy(&message.data[pos_in_msg], packet.data.message_data, len);
    bytes_received += len;

    message.type = static_cast<MessageType>(header.type);
    message.origin = packet.meta.origin;
    message.destination = packet.meta.destination;

    if (!more_fragments)
    {
        log_debug("Packet ", packet.to_string(), " is the last one of its message.");
        last_fragment_number = fragment_number;
    }

    return true;
}

Message &FragmentAssembler::assemble()
{
    message.length = bytes_received;
    return message;
}
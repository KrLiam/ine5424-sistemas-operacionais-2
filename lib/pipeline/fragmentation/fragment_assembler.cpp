#include "pipeline/fragmentation/fragment_assembler.h"

FragmentAssembler::FragmentAssembler() : bytes_received(0), last_fragment_number(INT_MAX), received_fragments()
{
}

FragmentAssembler::~FragmentAssembler()
{
}

bool FragmentAssembler::has_received(Packet &packet)
{
    return received_fragments.contains(packet.data.header.get_fragment_number());
}

bool FragmentAssembler::is_complete()
{
    return last_fragment_number == received_fragments.size() - 1;
}

void FragmentAssembler::add_packet(Packet &packet)
{
    if (has_received(packet))
    {
        log_trace("Ignoring duplicated ", packet.to_string(PacketFormat::RECEIVED), ".");
        return;
    };

    PacketMetadata& meta = packet.meta;
    PacketHeader& header = packet.data.header;

    uint32_t fragment_number = header.get_fragment_number();
    received_fragments.insert(fragment_number);

    unsigned int pos_in_msg = fragment_number * PacketData::MAX_MESSAGE_SIZE;
    unsigned int len = meta.message_length;
    memcpy(&message.data[pos_in_msg], packet.data.message_data, len);
    bytes_received += len;

    message.id = header.id;
    message.origin = meta.origin;
    message.destination = meta.destination;

    if (header.is_end())
    {
        log_trace("Packet ", packet.to_string(PacketFormat::RECEIVED), " is the last one of its message.");
        last_fragment_number = fragment_number;
    }
}

Message &FragmentAssembler::assemble()
{
    message.length = bytes_received;
    return message;
}
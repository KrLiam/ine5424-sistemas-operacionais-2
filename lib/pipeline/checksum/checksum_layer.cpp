#include "pipeline/checksum/checksum_layer.h"
#include "utils/log.h"

ChecksumLayer::ChecksumLayer(PipelineHandler handler) : PipelineStep(handler, nullptr) {}

ChecksumLayer::~ChecksumLayer() {}

void ChecksumLayer::send(Packet packet)
{
    log_trace("Packet ", packet.to_string(PacketFormat::SENT), " sent to checksum layer.");

    PacketData &data = packet.data;

    char buffer[PacketData::MAX_PACKET_SIZE];
    prepare_packet_buffer(data, packet.meta.message_length, buffer);

    unsigned short checksum = CRC16::calculate(buffer, PacketData::MAX_PACKET_SIZE);
    log_debug("Calculated checksum: ", checksum);

    packet.data.header.checksum = checksum;
    handler.forward_send(packet);
}

void ChecksumLayer::receive(Packet packet)
{
    log_trace("Packet ", packet.to_string(PacketFormat::RECEIVED), " received on checksum layer.");

    unsigned short received_checksum = packet.data.header.checksum;
    PacketData &data = packet.data;
    packet.data.header.checksum = 0;

    char buffer[PacketData::MAX_PACKET_SIZE];
    prepare_packet_buffer(data, packet.meta.message_length, buffer);

    unsigned short calculated_checksum = CRC16::calculate(buffer, PacketData::MAX_PACKET_SIZE);

    if (calculated_checksum == received_checksum) {
        handler.forward_receive(packet);
    }
    else {
        log_warn("Checksum is different: Expected ", received_checksum, ", got ", calculated_checksum);
    }
}

void ChecksumLayer::prepare_packet_buffer(const PacketData &packet_data, std::size_t message_length, char *buffer)
{
    memset(buffer, 0, PacketData::MAX_PACKET_SIZE);
    memcpy(buffer, &packet_data.header, sizeof(PacketHeader));
    memcpy(buffer + sizeof(PacketHeader), packet_data.message_data, message_length);
}
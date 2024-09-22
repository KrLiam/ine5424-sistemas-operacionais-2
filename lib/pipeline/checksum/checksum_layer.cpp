#include "pipeline/checksum/checksum_layer.h"
#include "utils/log.h"

ChecksumLayer::ChecksumLayer(PipelineHandler handler) : PipelineStep(handler, nullptr) {}

ChecksumLayer::~ChecksumLayer() {}

void ChecksumLayer::send(Packet packet) {
    log_trace("Packet ", packet.to_string(PacketFormat::SENT), " sent to checksum layer.");

    PacketData& data = packet.data;

    // calcular checksum somente do PacketData
    unsigned int checksum = 0;

    packet.data.header.checksum = checksum;
    handler.forward_send(packet);

    IGNORE_UNUSED(data);
    IGNORE_UNUSED(packet);
    IGNORE_UNUSED(checksum);
}

void ChecksumLayer::receive(Packet packet) {
    log_trace("Packet ", packet.to_string(PacketFormat::RECEIVED), " received on checksum layer.");

    unsigned int checksum = packet.data.header.checksum;
    PacketData& data = packet.data;

    // validar checksum
    bool valid = true;

    if (valid) {
        handler.forward_receive(packet);
    }

    IGNORE_UNUSED(data);
    IGNORE_UNUSED(checksum);
}
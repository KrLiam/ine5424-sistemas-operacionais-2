#include "pipeline/checksum/checksum_layer.h"
#include "utils/log.h"

ChecksumLayer::ChecksumLayer(PipelineHandler handler) : PipelineStep(handler, nullptr) {}

ChecksumLayer::~ChecksumLayer() {}

void ChecksumLayer::send(Packet packet) {
    log_trace("Packet [", packet.to_string(), "] sent to checksum layer.");

    PacketData& data = packet.data;

    // calcular checksum somente do PacketData
    unsigned int checksum = 0;

    packet.data.header.checksum = checksum;
    handler.forward_send(packet);
}

void ChecksumLayer::receive(Packet packet) {
    log_trace("Packet [", packet.to_string(), "] received on checksum layer.");

    unsigned int checksum = packet.data.header.checksum;
    PacketData& data = packet.data;

    // validar checksum
    bool valid = true;

    if (valid) {
        handler.forward_receive(packet);
    }
}
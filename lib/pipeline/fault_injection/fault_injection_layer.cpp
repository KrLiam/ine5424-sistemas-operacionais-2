#include <random>

#include "pipeline/fault_injection/fault_injection_layer.h"
#include "utils/log.h"

bool roll_chance(double chance) {
    double value = ((double) std::rand()) / RAND_MAX;
    return value < chance;
}

FaultInjectionLayer::FaultInjectionLayer(PipelineHandler handler) : FaultInjectionLayer(handler, 0, 0, 0) {}
FaultInjectionLayer::FaultInjectionLayer(
    PipelineHandler handler, int min_delay, int max_delay, double lose_chance
) :
    PipelineStep(handler, nullptr),
    min_delay(min_delay),
    max_delay(max_delay),
    lose_chance(lose_chance)
    {}

void FaultInjectionLayer::receive(Packet packet) {
    unsigned int msg_num = packet.data.header.msg_num;
    unsigned int fragment_num = packet.data.header.fragment_num;

    // lose packet entirely
    if (roll_chance(lose_chance)) {
        log_debug("Lost packet ", msg_num, "/", fragment_num, ".");
        return;
    };

    int range_length = max_delay - min_delay;

    if (range_length) {
        int delay = min_delay + std::rand() % range_length;
        log_debug("Reception of packet ", msg_num, "/", fragment_num, " is delayed by ", delay, " ms.");
        
        timer.add(delay, [this, packet]() {
            proceed_receive(packet);
        });
    }
    else {
        proceed_receive(packet);
    }
}

void FaultInjectionLayer::proceed_receive(Packet packet) {
    PacketHeader& header = packet.data.header;
    unsigned int msg_num = packet.data.header.msg_num;
    unsigned int fragment_num = packet.data.header.fragment_num;

    log_info("Received packet ", packet.to_string(), " (", packet.meta.message_length, " bytes).");
    handler.forward_receive(packet);
}
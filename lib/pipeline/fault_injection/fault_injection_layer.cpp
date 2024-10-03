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
    PipelineStep(handler),
    min_delay(min_delay),
    max_delay(max_delay),
    lose_chance(lose_chance)
    {}

void FaultInjectionLayer::enqueue_fault(int delay = INT_MAX) {
    mutex_fault_queue.lock();
    fault_queue.push_back(delay);
    mutex_fault_queue.unlock();
}

void FaultInjectionLayer::enqueue_fault(const std::vector<int>& faults) {
    mutex_fault_queue.lock();
    for (const int delay : faults) {
        fault_queue.push_back(delay);
    }
    mutex_fault_queue.unlock();
}

void FaultInjectionLayer::receive(Packet packet) {
    int delay = -1;

    if (fault_queue.size()) {
        mutex_fault_queue.lock();

        delay = fault_queue.at(0);
        fault_queue.erase(fault_queue.begin());

        mutex_fault_queue.unlock();
    }

    // lose packet entirely
    if (delay == INT_MAX || (delay == -1 && roll_chance(lose_chance))) {
        log_warn("Lost ", packet.to_string(PacketFormat::RECEIVED));
        return;
    };

    int range_length = max_delay - min_delay;
    if (delay == -1 && range_length) {
        delay = min_delay + std::rand() % range_length;
        [[maybe_unused]] unsigned int msg_num = packet.data.header.id.msg_num;
        [[maybe_unused]] unsigned int fragment_num = packet.data.header.fragment_num;
        
        log_debug(
            "Reception of packet ",
            msg_num,
            "/",
            fragment_num,
            " is delayed by ",
            delay,
            " ms."
        );
    }

    if (delay > 0) {
        timer.add(delay, [this, packet]() {
            proceed_receive(packet);
        });
    }
    else {
        proceed_receive(packet);
    }
}

void FaultInjectionLayer::proceed_receive(Packet packet) {
    log_info("Received ", packet.to_string(PacketFormat::RECEIVED), " (", packet.meta.message_length, " bytes).");
    handler.forward_receive(packet);
}
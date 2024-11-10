#include "pipeline/fault_injection/fault_injection_layer.h"
#include "utils/log.h"
#include "utils/random.h"

bool roll_chance(double chance) {
    double value = ((double) rc_random::dis(rc_random::gen)) / RAND_MAX;
    return value < chance;
}

FaultInjectionLayer::FaultInjectionLayer(PipelineHandler handler) : FaultInjectionLayer(handler, FaultConfig()) {}
FaultInjectionLayer::FaultInjectionLayer(
    PipelineHandler handler, FaultConfig config
) :
    PipelineStep(handler),
    config(config),
    corruption_mask_dis(0, 255)
{
    if (config.lose_chance > 0) {
        DropFaultRule rule{
            pattern: {number: IntRange::full(), fragment: IntRange::full(), sequence_types: {'u','b','a','h'}, flags: 0},
            chance: config.lose_chance,
            count: UINT32_MAX,
        };
        add_rule(rule);
    }
}

FaultInjectionLayer::~FaultInjectionLayer() {
    for (int id : packet_timer_ids) {
        TIMER.cancel(id);
    }
}

void FaultInjectionLayer::discard_drop_rules(const PacketPattern& pattern) {
    for (int i = 0; i < (int) drop_rules.size(); i++) {
        const DropFaultRule& rule = drop_rules[i];

        if (pattern.contains(rule.pattern)) {
            drop_rules.erase(drop_rules.begin() + i);
            i--;
        }
    }
}

void FaultInjectionLayer::decrement_matching_drop_rules(const PacketPattern& pattern) {
    for (int i = 0; i < (int) drop_rules.size(); i++) {
        DropFaultRule& rule = drop_rules[i];

        if (rule.count == UINT32_MAX) continue;

        if (rule.pattern.contains(pattern)) {
            if (rule.count > 1) {
                rule.count--;
            }
            else {
                drop_rules.erase(drop_rules.begin() + i);
                i--;
            }
        }
    }
}

void FaultInjectionLayer::add_rule(const FaultRule& rule) {
    if (auto value = std::get_if<DropFaultRule>(&rule)) {
        discard_drop_rules(value->pattern);
        drop_rules.push_back(*value);
    }
}

void FaultInjectionLayer::receive(Packet packet) {    
    const MessageIdentity id = packet.data.header.id;
    uint32_t fragment = packet.data.header.fragment_num;
    uint32_t flags = packet.data.header.flags;

    for (size_t i = 0; i < drop_rules.size(); i++) {
        const DropFaultRule& rule = drop_rules[i];

        if (!rule.pattern.matches(id, fragment, flags)) continue;

        if (roll_chance(rule.chance)) {
            log_warn("Lost ", packet.to_string(PacketFormat::RECEIVED), " (rule ", rule.to_string(), ")");

            decrement_matching_drop_rules(PacketPattern::from(id, fragment, flags));
            return;
        }
    }

    int delay = -1;

    int range_length = config.delay.length();
    int offset = range_length > 1 ? std::rand() % range_length : 0;
    delay = config.delay.min + offset;
    [[maybe_unused]] unsigned int msg_num = packet.data.header.id.msg_num;
    [[maybe_unused]] unsigned int fragment_num = packet.data.header.fragment_num;

    if (!packet.silent()) {
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
        int id = TIMER.add(delay, [this, packet]() {
            proceed_receive(packet);
        });
        packet_timer_ids.emplace(id);
    }
    else {
        proceed_receive(packet);
    }
}

void FaultInjectionLayer::proceed_receive(Packet packet) {
    if (roll_chance(config.corrupt_chance)) corrupt(packet);

    if (!packet.silent()) {
        log_info("Received ", packet.to_string(PacketFormat::RECEIVED), " (", packet.meta.message_length, " bytes).");
    }
    handler.forward_receive(packet);
}

void FaultInjectionLayer::corrupt(Packet& packet) {
    if (!packet.silent()) {
        log_warn("Corrupting ", packet.to_string(PacketFormat::RECEIVED), ".");
    }

    char new_header[sizeof(packet.data.header)];
    memcpy(new_header, &packet.data.header, sizeof(new_header));
    for (int i = 0; i < sizeof(packet.data.header); i++) {
        char mask = corruption_mask_dis(rc_random::gen);
        new_header[i] = new_header[i] ^ mask;
    }
    memcpy(&packet.data.header, new_header, sizeof(packet.data.header));
    
    for (int i = 0; i < packet.meta.message_length; i++) {
        char mask = corruption_mask_dis(rc_random::gen);
        packet.data.message_data[i] = packet.data.message_data[i] ^ mask;
    }
}

#pragma once

#include <mutex>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/config.h"
#include "utils/date.h"


bool roll_chance(double chance);

enum class FaultRuleType {
    LOSE = 0,
    DELAY = 1
};

// delay * 0s, delay * 1s
// lose * once, lose 1u once

// lose * 5%

struct PacketPattern {
    IntRange number;
    IntRange fragment;
    std::unordered_set<char> sequence_types;

    static PacketPattern from(const MessageIdentity& id, uint32_t frag_num) {
        return PacketPattern{
            IntRange(id.msg_num),
            IntRange(frag_num),
            {(char) id.sequence_type()}
        };
    }

    std::string to_string() const {
        std::string types;
        for (char ch : sequence_types) {
            types += ch;
        }

        return format(
            "%s/%s%s", 
            number.to_string().c_str(),
            fragment.to_string().c_str(),
            types.c_str()
        );
    }

    bool wildcard() const {
        return number == IntRange::full()
            && fragment == IntRange::full()
            && sequence_types.size() == 3;
    }

    bool contains(const PacketPattern& other) const {
        for (char type : other.sequence_types) {
            if (!sequence_types.contains(type)) return false;
        }

        return number.contains(other.number)
            && fragment.contains(other.fragment);
    }

    bool matches(const MessageIdentity& id, uint32_t frag_num) const {
        return contains(PacketPattern::from(id, frag_num));
    }
};

struct DropFaultRule {
    PacketPattern pattern;
    double chance = 1.0;
    uint32_t count = 1;

    std::string to_string() const {
        std::string count_s = count == UINT32_MAX ? std::string("") : format("%ux", count);

        return format(
            "%s %.2f%% %s",
            pattern.to_string().c_str(),
            chance * 100,
            count_s.c_str()
        );
    }
};

struct DelayFaultRule {
    PacketPattern pattern;
    uint32_t count;
    IntRange delay;

};

using FaultRule = std::variant<DropFaultRule, DelayFaultRule>;


class FaultInjectionLayer : public PipelineStep {
    std::vector<DropFaultRule> drop_rules;
    std::unordered_set<int> packet_timer_ids;
   FaultConfig config;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, FaultConfig config);

    ~FaultInjectionLayer();

    void discard_drop_rules(const PacketPattern& pattern);
    void decrement_matching_drop_rules(const PacketPattern& pattern);

    void add_rule(const FaultRule& rule);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};
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

    bool wildcard() const {
        return number == IntRange::full()
            && fragment == IntRange::full()
            && sequence_types.size() == 3;
    }

    bool contains(const PacketPattern& other) const {
        bool contains_types = true;
        for (char type : other.sequence_types) {
            contains_types = sequence_types.contains(type);
            if (!contains_types) return false;
        }

        return number.contains(other.number)
            && fragment.contains(other.fragment);
    }

    bool matches(const MessageIdentity& id, uint32_t frag_num) const {
        if (!number.contains(id.msg_num)) return false;
        if (!fragment.contains(frag_num)) return false;
        if (!sequence_types.contains((char)id.sequence_type())) return false;

        return true;
    }
};

struct DropFaultRule {
    PacketPattern pattern;
    double chance = 1;
    uint32_t count = 1;
};

struct DelayFaultRule {
    PacketPattern pattern;
    uint32_t count;
    IntRange delay;
};

using FaultRule = std::variant<DropFaultRule, DelayFaultRule>;


class FaultInjectionLayer : public PipelineStep {
    std::vector<DropFaultRule> drop_rules;
   FaultConfig config;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, FaultConfig config);

    void discard_drop_rules(const PacketPattern& pattern);
    void decrement_drop_rules(const PacketPattern& pattern);

    void add_rule(const FaultRule& rule);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};
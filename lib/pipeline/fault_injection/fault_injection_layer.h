#pragma once

#include <variant>
#include <mutex>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/config.h"
#include "utils/date.h"


bool roll_chance(double chance);

struct PacketPattern {
    IntRange number;
    IntRange fragment;
    std::unordered_set<char> sequence_types;
    uint32_t flags = 0;
    std::string source;

    static PacketPattern from(const MessageIdentity& id, uint32_t frag_num, uint32_t flags, const SocketAddress& source) {
        return PacketPattern{
            IntRange(id.msg_num),
            IntRange(frag_num),
            {(char) id.sequence_type()},
            flags,
            source.to_string()
        };
    }

    std::string to_string() const {
        std::string types;
        for (char ch : sequence_types) {
            types += ch;
        }

        return format(
            "%s/%s%s from %s",
            number.to_string().c_str(),
            fragment.to_string().c_str(),
            types.c_str(),
            source.c_str()
        );
    }

    bool contains(const PacketPattern& other) const {
        for (char type : other.sequence_types) {
            if (!sequence_types.contains(type)) return false;
        }

        if ((flags & other.flags) != flags) return false;

        return number.contains(other.number)
            && fragment.contains(other.fragment)
            && (source == "*" || source == other.source);
    }

    bool matches(const MessageIdentity& id, uint32_t frag_num, uint32_t flags, const SocketAddress& source) const {
        return contains(PacketPattern::from(id, frag_num, flags, source));
    }
};

struct DropFaultRule {
    PacketPattern pattern;
    double chance = 1.0;
    uint32_t count = 1;

    std::string to_string() const {
        std::string count_s = count == UINT32_MAX ? std::string("") : format("%ux", count);

        return format(
            "%s %.0f%% %s",
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
   std::uniform_int_distribution<> corruption_mask_dis;
public:
    FaultInjectionLayer(PipelineHandler handler);
    FaultInjectionLayer(PipelineHandler handler, FaultConfig config);

    ~FaultInjectionLayer();

    void discard_drop_rules(const PacketPattern& pattern);
    void decrement_matching_drop_rules(const PacketPattern& pattern);

    void corrupt(Packet& packet);

    void add_rule(const FaultRule& rule);

    void receive(Packet packet);

    void proceed_receive(Packet packet);
};
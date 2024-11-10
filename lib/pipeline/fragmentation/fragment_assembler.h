#pragma once

#include <unordered_set>
#include <vector>

#include "core/event.h"
#include "core/event_bus.h"
#include "core/message.h"
#include "core/packet.h"
#include "utils/date.h"
#include "utils/log.h"

class FragmentAssembler
{
    static const int MESSAGE_TIMEOUT = 30000;

    EventBus &event_bus;

    unsigned int bytes_received;
    uint32_t last_fragment_number;
    std::unordered_set<uint32_t> received_fragments;
    Message message{};
    int timer_id;

    void message_timeout();

public:
    FragmentAssembler(EventBus &event_bus);
    ~FragmentAssembler();

    bool has_received(Packet&);
    bool is_complete();
    void add_packet(Packet&);
    Message &assemble();
};
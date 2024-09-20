#pragma once

#include <unordered_set>
#include <vector>

#include "core/message.h"
#include "core/packet.h"
#include "utils/log.h"

class FragmentAssembler
{
    unsigned int bytes_received;
    uint32_t last_fragment_number;
    std::unordered_set<uint32_t> received_fragments;
    Message message{};

public:
    FragmentAssembler();
    ~FragmentAssembler();

    bool has_received(Packet&);
    bool is_complete();
    void add_packet(Packet&);
    Message &assemble();
};
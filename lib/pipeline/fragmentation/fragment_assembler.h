#pragma once

#include <vector>

#include "core/message.h"
#include "core/packet.h"
#include "utils/log.h"

class FragmentAssembler
{
    unsigned int bytes_received;
    uint32_t last_fragment_number;
    std::vector<uint32_t> received_fragments;
    Message message{};

public:
    FragmentAssembler();
    ~FragmentAssembler();

    bool has_received_all_packets();
    void add_packet(Packet packet);
    Message &assemble();
};
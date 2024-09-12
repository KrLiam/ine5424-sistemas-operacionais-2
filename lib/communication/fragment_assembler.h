#pragma once

#include <vector>

#include "core/segment.h"
#include "utils/log.h"

class FragmentAssembler
{
private:
    unsigned int bytes_received;
    uint32_t last_fragment_number;
    std::vector<uint32_t> received_fragments;
    Message message;

public:
    FragmentAssembler();
    ~FragmentAssembler();

    bool has_received(Packet packet);
    bool has_received_all_packets();
    void add_packet(Packet packet);
    Message &assemble();
};

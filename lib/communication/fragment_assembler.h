#pragma once

#include <vector>
#include <unordered_set>

#include "core/segment.h"
#include "utils/log.h"

class FragmentAssembler
{
private:
    unsigned int bytes_received;
    uint32_t last_fragment_number;
    std::unordered_set<uint32_t> received_fragments;
    Message message;

public:
    FragmentAssembler();
    ~FragmentAssembler();

    bool has_received(const Packet& packet);
    bool is_complete();
    bool add_packet(const Packet& packet);
    Message &assemble();
};

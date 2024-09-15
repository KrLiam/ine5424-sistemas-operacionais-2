#include "core/segment.h"

class Fragmenter {
    const Message& message;
    unsigned int i = 0;

    int total_fragments;

    Packet create_packet();

public:
    Fragmenter(const Message& message);

    int get_total_fragments();

    bool has_next();

    bool next(Packet* packet);
};
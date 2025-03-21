#include "core/message.h"
#include "core/packet.h"

class Fragmenter {
    const Message& message;
    unsigned int i = 0;

    unsigned int total_fragments;

    Packet create_packet();

public:
    Fragmenter(const Message& message);

    unsigned int get_total_fragments();

    bool has_next();

    bool next(Packet* packet);
};
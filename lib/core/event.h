#pragma once

enum EventType {
    BASE = -1,
    PACKET_ACK_RECEIVED = 0
};

struct Event {
    static EventType type() { return EventType::BASE; }
};

struct PacketAckReceived : public Event {
    static EventType type() { return EventType::PACKET_ACK_RECEIVED; }

    Packet& ack_packet;
};

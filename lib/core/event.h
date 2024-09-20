#pragma once

#include "core/packet.h"

enum EventType {
    BASE = -1,
    PACKET_ACK_RECEIVED = 0,
    MESSAGE_DEFRAGMENTATION_IS_COMPLETE = 2,
    FORWARD_DEFRAGMENTED_MESSAGE = 3
};

struct Event {
    static EventType type() { return EventType::BASE; }

protected:
    Event();
};

struct PacketAckReceived : public Event {
    static EventType type() { return EventType::PACKET_ACK_RECEIVED; }

    Packet& ack_packet;

    PacketAckReceived(Packet& ack_packet);
};

struct MessageDefragmentationIsComplete : public Event {
    static EventType type() { return EventType::MESSAGE_DEFRAGMENTATION_IS_COMPLETE; }

    Packet& packet; // TODO: Dá pra trocar pelo UUID da mensagem depois de implementarmos

    MessageDefragmentationIsComplete(Packet& packet);
};

struct ForwardDefragmentedMessage : public Event {
    static EventType type() { return EventType::FORWARD_DEFRAGMENTED_MESSAGE; }

    Packet& packet; // TODO: Dá pra trocar pelo UUID da mensagem depois de implementarmos

    ForwardDefragmentedMessage(Packet& packet);
};

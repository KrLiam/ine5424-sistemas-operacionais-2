#pragma once

#include "core/packet.h"

enum EventType {
    BASE = -1,
    PACKET_ACK_RECEIVED = 0,
    TRANSMISSION_FAIL = 1,
    TRANSMISSION_COMPLETE = 2,
    MESSAGE_DEFRAGMENTATION_IS_COMPLETE = 3,
    FORWARD_DEFRAGMENTED_MESSAGE = 4
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

struct TransmissionFail : public Event {
    static EventType type() { return EventType::TRANSMISSION_FAIL; }

    Packet& faulty_packet;

    TransmissionFail(Packet& faulty_packet);
};

struct TransmissionComplete : public Event {
    static EventType type() { return EventType::TRANSMISSION_FAIL; }

    SocketAddress remote_address;
    uint32_t msg_num;

    TransmissionComplete(const SocketAddress& remote_address, uint32_t msg_num);
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

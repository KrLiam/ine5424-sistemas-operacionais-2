#pragma once

#include "core/packet.h"
#include "core/node.h"

enum EventType {
    BASE = -1,
    PACKET_ACK_RECEIVED = 0,
    TRANSMISSION_FAIL = 1,
    TRANSMISSION_COMPLETE = 2,
    MESSAGE_DEFRAGMENTATION_IS_COMPLETE = 3,
    FORWARD_DEFRAGMENTED_MESSAGE = 4,
    PIPELINE_CLEANUP = 5,
    CONNECTION_ESTABLISHED = 6,
    CONNECTION_FAIL = 7,
    HEARTBEAT_RECEIVED = 8,
    NODE_DEATH = 9,
};

struct Event {
    static EventType type() { return EventType::BASE; }

protected:
    Event();
};

struct ConnectionEstablished : public Event {
    static EventType type() { return EventType::CONNECTION_ESTABLISHED; }

    const Node& node;

    ConnectionEstablished(const Node& node);
};

struct ConnectionClosed : public Event {
    static EventType type() { return EventType::CONNECTION_FAIL; }

    const Node& node;

    ConnectionClosed(const Node& node);
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
    static EventType type() { return EventType::TRANSMISSION_COMPLETE; }

    UUID uuid;
    SocketAddress remote_address;
    uint32_t msg_num;

    TransmissionComplete(UUID transmission_uuid, const SocketAddress& remote_address, uint32_t msg_num);
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

struct PipelineCleanup : public Event {
    static EventType type() { return EventType::PIPELINE_CLEANUP; }

    Message& message;

    PipelineCleanup(Message& message);
};

struct HeartbeatReceived : public Event {
    static EventType type() { return EventType::HEARTBEAT_RECEIVED; }

    Node& remote_node;

    HeartbeatReceived(Node& remote_node);
};

struct NodeDeath: public Event {
    static EventType type() { return EventType::NODE_DEATH; }

    const Node& remote_node;

    NodeDeath(const Node& remote_node);
};
#pragma once

#include <unordered_set>

#include "core/packet.h"
#include "core/node.h"

enum EventType {
    BASE = -1,
    PACKET_RECEIVED = 0,
    PACKET_ACK_RECEIVED = 1,
    MESSAGE_RECEIVED = 2,
    TRANSMISSION_STARTED = 3,
    TRANSMISSION_FAIL = 4,
    TRANSMISSION_COMPLETE = 5,
    MESSAGE_DEFRAGMENTATION_IS_COMPLETE = 6,
    FORWARD_DEFRAGMENTED_MESSAGE = 7,
    PIPELINE_CLEANUP = 8,
    CONNECTION_ESTABLISHED = 9,
    CONNECTION_FAIL = 10,
    HEARTBEAT_RECEIVED = 11,
    NODE_DEATH = 12,
    FRAGMENT_RECEIVED = 13,
    UNICAST_MESSAGE_RECEIVED = 14,
    RECEIVE_SYNCHRONIZATION = 15,
    LEADER_ELECTED = 16
};
    
struct Event {
    static EventType type() { return EventType::BASE; }

protected:
    Event();
};

struct ReceiveSynchronization : public Event {
    static EventType type() { return EventType::RECEIVE_SYNCHRONIZATION; }

    const Node& node;
    uint32_t expected_number;
    uint32_t expected_broadcast_number;
    uint32_t expected_ab_number;

    ReceiveSynchronization(const Node& node, uint32_t expected_number, uint32_t expected_broadcast_number, uint32_t expected_ab_number);
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

struct PacketReceived : public Event {
    static EventType type() { return EventType::PACKET_RECEIVED; }

    Packet& packet;

    PacketReceived(Packet& packet);
};

struct FragmentReceived : public Event {
    static EventType type() { return EventType::FRAGMENT_RECEIVED; }

    Packet& packet;

    FragmentReceived(Packet& packet);
};

struct PacketAckReceived : public Event {
    static EventType type() { return EventType::PACKET_ACK_RECEIVED; }

    Packet& ack_packet;

    PacketAckReceived(Packet& ack_packet);
};

struct MessageReceived : public Event {
    static EventType type() { return EventType::MESSAGE_RECEIVED; }

    Message& message;

    MessageReceived(Message& message);
};

struct UnicastMessageReceived : public Event {
    static EventType type() { return EventType::UNICAST_MESSAGE_RECEIVED; }

    Message& message;

    UnicastMessageReceived(Message& message);
};

struct TransmissionStarted : public Event {
    static EventType type() { return EventType::TRANSMISSION_STARTED; }

    const Message& message;

    TransmissionStarted(const Message& message);
};

struct TransmissionFail : public Event {
    static EventType type() { return EventType::TRANSMISSION_FAIL; }

    UUID uuid;
    MessageIdentity id;
    std::unordered_set<const Packet*> faulty_packets;
    std::unordered_set<const Node*> faulty_nodes;

    TransmissionFail(
        const UUID& uuid,
        const MessageIdentity& id,
        const std::unordered_set<const Packet*>& faulty_packets,
        const std::unordered_set<const Node*>& faulty_nodes
    );
};

struct TransmissionComplete : public Event {
    static EventType type() { return EventType::TRANSMISSION_COMPLETE; }

    UUID uuid;
    MessageIdentity id;
    SocketAddress remote_address;

    TransmissionComplete(UUID transmission_uuid, const MessageIdentity& id, const SocketAddress& remote_address);
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

struct LeaderElected: public Event {
    static EventType type() { return EventType::LEADER_ELECTED; }

    LeaderElected();
};

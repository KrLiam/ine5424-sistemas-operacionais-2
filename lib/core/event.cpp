#include "core/event.h"

Event::Event() {}

ReceiveSynchronization::ReceiveSynchronization(const Node& node, uint32_t expected_number, uint32_t expected_broadcast_number)
    : node(node), expected_number(expected_number), expected_broadcast_number(expected_broadcast_number) {}

ConnectionEstablished::ConnectionEstablished(const Node& node)
    : node(node) {}

ConnectionClosed::ConnectionClosed(const Node& node) : node(node) {}

PacketReceived::PacketReceived(Packet& packet) : packet(packet) {}

FragmentReceived::FragmentReceived(Packet& packet) : packet(packet) {}

PacketAckReceived::PacketAckReceived(Packet& ack_packet) : ack_packet(ack_packet) {}

MessageReceived::MessageReceived(Message& message) : message(message) {}

DeliverMessage::DeliverMessage(Message& message) : message(message) {}

TransmissionStarted::TransmissionStarted(const Message& message)
    : message(message) {}

TransmissionFail::TransmissionFail(Packet& faulty_packet) : faulty_packet(faulty_packet) {}

TransmissionComplete::TransmissionComplete(UUID uuid, const MessageIdentity& id, const SocketAddress& remote_address)
    : uuid(uuid), id(id), remote_address(remote_address) {}

MessageDefragmentationIsComplete::MessageDefragmentationIsComplete(Packet& packet) : packet(packet) {}

ForwardDefragmentedMessage::ForwardDefragmentedMessage(Packet& packet) : packet(packet) {}

PipelineCleanup::PipelineCleanup(Message& message) : message(message) {}

HeartbeatReceived::HeartbeatReceived(Node& remote_node) : remote_node(remote_node) {}

NodeDeath::NodeDeath(const Node& remote_node) : remote_node(remote_node) {}
#include "core/event.h"

Event::Event() {}

ConnectionEstablished::ConnectionEstablished(const Node& node) : node(node) {}

ConnectionClosed::ConnectionClosed(const Node& node) : node(node) {}

PacketReceived::PacketReceived(Packet& packet) : packet(packet) {}

PacketAckReceived::PacketAckReceived(Packet& ack_packet) : ack_packet(ack_packet) {}

MessageReceived::MessageReceived(Message& message) : message(message) {}

TransmissionStarted::TransmissionStarted(const Message& message)
    : message(message) {}

TransmissionFail::TransmissionFail(Packet& faulty_packet) : faulty_packet(faulty_packet) {}

TransmissionComplete::TransmissionComplete(UUID uuid, const SocketAddress& remote_address, uint32_t msg_num)
    : uuid(uuid), remote_address(remote_address), msg_num(msg_num) {}

MessageDefragmentationIsComplete::MessageDefragmentationIsComplete(Packet& packet) : packet(packet) {}

ForwardDefragmentedMessage::ForwardDefragmentedMessage(Packet& packet) : packet(packet) {}

PipelineCleanup::PipelineCleanup(Message& message) : message(message) {}

HeartbeatReceived::HeartbeatReceived(Node& remote_node) : remote_node(remote_node) {}

NodeDeath::NodeDeath(const Node& remote_node) : remote_node(remote_node) {}
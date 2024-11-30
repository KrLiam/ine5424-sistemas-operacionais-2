#include "core/event.h"

Event::Event() {}

ReceiveSynchronization::ReceiveSynchronization(const Node& node, uint32_t expected_number, uint32_t expected_broadcast_number, uint32_t expected_ab_number)
    : node(node), expected_number(expected_number), expected_broadcast_number(expected_broadcast_number), expected_ab_number(expected_ab_number) {}

ConnectionEstablished::ConnectionEstablished(const Node& node)
    : node(node) {}

ConnectionClosed::ConnectionClosed(const Node& node) : node(node) {}

PacketReceived::PacketReceived(Packet& packet) : packet(packet) {}

PacketSent::PacketSent(Packet& packet) : packet(packet) {}

FragmentReceived::FragmentReceived(Packet& packet) : packet(packet) {}

PacketAckReceived::PacketAckReceived(Packet& ack_packet) : ack_packet(ack_packet) {}

MessageReceived::MessageReceived(Message& message) : message(message) {}

UnicastMessageReceived::UnicastMessageReceived(Message& message) : message(message) {}

TransmissionStarted::TransmissionStarted(const Message& message)
    : message(message) {}

TransmissionFail::TransmissionFail(
    const UUID& uuid,
    const MessageIdentity& id,
    const std::unordered_set<const Packet*>& faulty_packets,
    const std::unordered_set<const Node*>& faulty_nodes
)
    : uuid(uuid), id(id), faulty_packets(faulty_packets), faulty_nodes(faulty_nodes) {}

TransmissionComplete::TransmissionComplete(UUID uuid, const MessageIdentity& id, const SocketAddress& source, const SocketAddress& remote_address)
    : uuid(uuid), id(id), source(source), remote_address(remote_address) {}

MessageDefragmentationIsComplete::MessageDefragmentationIsComplete(Packet& packet) : packet(packet) {}

ForwardDefragmentedMessage::ForwardDefragmentedMessage(Packet& packet) : packet(packet) {}

PipelineCleanup::PipelineCleanup(const MessageIdentity& id, const SocketAddress& destination) : id(id), destination(destination) {}

NodeDeath::NodeDeath(const Node& remote_node) : remote_node(remote_node) {}

NodeUp::NodeUp(const Node& remote_node) : remote_node(remote_node) {}

LeaderElected::LeaderElected() {}

AtomicMapping::AtomicMapping(const MessageIdentity& request_id, const MessageIdentity& atomic_id) : request_id(request_id), atomic_id(atomic_id) {}

JoinGroup::JoinGroup(const std::string &id, const ByteArray &key) : id(id), key(key) {}

LeaveGroup::LeaveGroup(const std::string& id, const uint64_t &key_hash) : id(id), key_hash(key_hash) {}

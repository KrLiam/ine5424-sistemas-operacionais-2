#include "core/event.h"

Event::Event() {}

PacketAckReceived::PacketAckReceived(Packet& ack_packet) : ack_packet(ack_packet) {}

TransmissionFail::TransmissionFail(Packet& faulty_packet) : faulty_packet(faulty_packet) {}

TransmissionComplete::TransmissionComplete(UUID uuid, const SocketAddress& remote_address, uint32_t msg_num)
    : uuid(uuid), remote_address(remote_address), msg_num(msg_num) {}

MessageDefragmentationIsComplete::MessageDefragmentationIsComplete(Packet& packet) : packet(packet) {}

ForwardDefragmentedMessage::ForwardDefragmentedMessage(Packet& packet) : packet(packet) {}

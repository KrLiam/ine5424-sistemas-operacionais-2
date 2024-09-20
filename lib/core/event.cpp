#include "core/event.h"

Event::Event() {}

PacketAckReceived::PacketAckReceived(Packet& ack_packet) : ack_packet(ack_packet) {}


#include "communication/broadcast_connection.h"

BroadcastConnection::BroadcastConnection(
    std::map<std::string, Connection>& connections,
    BufferSet<std::string>& connection_update_buffer,
    Buffer<Message>& deliver_buffer,
    Pipeline& pipeline
) :
    connections(connections),
    pipeline(pipeline),
    dispatcher(BROADCAST_ID, connection_update_buffer, pipeline),
    connection_update_buffer(connection_update_buffer),
    deliver_buffer(deliver_buffer)
{
    observe_pipeline();
}

const TransmissionDispatcher& BroadcastConnection::get_dispatcher() const { return dispatcher; }

void BroadcastConnection::observe_pipeline() {
    obs_connection_established.on(std::bind(&BroadcastConnection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&BroadcastConnection::connection_closed, this, _1));
    obs_deliver_message.on(std::bind(&BroadcastConnection::deliver_message, this, _1));
    obs_fragment_received.on(std::bind(&BroadcastConnection::fragment_received, this, _1));
    obs_packet_ack_received.on(std::bind(&BroadcastConnection::packet_ack_received, this, _1));
    obs_transmission_fail.on(std::bind(&BroadcastConnection::transmission_fail, this, _1));
    obs_transmission_complete.on(std::bind(&BroadcastConnection::transmission_complete, this, _1));
    pipeline.attach(obs_connection_established);
    pipeline.attach(obs_connection_closed);
    pipeline.attach(obs_deliver_message);
    pipeline.attach(obs_fragment_received);
    pipeline.attach(obs_packet_ack_received);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
}

void BroadcastConnection::connection_established(const ConnectionEstablished&) {
    request_update();
}

void BroadcastConnection::connection_closed(const ConnectionClosed&) {
    dispatcher.cancel_all();
}

void BroadcastConnection::fragment_received(const FragmentReceived &event)
{
    if (event.packet.data.header.id.sequence_type != MessageSequenceType::BROADCAST) return;
    
    MessageType type = event.packet.data.header.type;

    if (type == MessageType::URB) {
        Packet packet = event.packet;
        packet.meta.destination = {BROADCAST_ADDRESS, 0};
        packet.meta.expects_ack = true;
        packet.meta.urb_retransmission = true;
        pipeline.send(packet);
    }
}

void BroadcastConnection::packet_ack_received(const PacketAckReceived &event)
{
    if (event.ack_packet.data.header.id.sequence_type != MessageSequenceType::BROADCAST) return;
    
    MessageType type = event.ack_packet.data.header.type;
}

void BroadcastConnection::deliver_message(const DeliverMessage &event)
{
    if (event.message.id.sequence_type != MessageSequenceType::BROADCAST) return;
    
    MessageType type = event.message.type;

    if (type == MessageType::BEB) {
        deliver_buffer.produce(event.message);
    }
    else if (type == MessageType::URB) {

    }
}

void BroadcastConnection::transmission_complete(const TransmissionComplete& event) {
    const UUID &uuid = event.uuid;
    const Transmission* active = dispatcher.get_active();

    if (active && uuid == active->uuid) dispatcher.complete(true);
}

void BroadcastConnection::transmission_fail(const TransmissionFail& event) {
    const UUID &uuid = event.faulty_packet.meta.transmission_uuid;
    const Transmission* active = dispatcher.get_active();

    if (active && uuid == active->uuid) dispatcher.complete(false);
}

void BroadcastConnection::request_update() {
    connection_update_buffer.produce(BROADCAST_ID);
}

bool BroadcastConnection::enqueue(Transmission& transmission) {
    return dispatcher.enqueue(transmission);
}

void BroadcastConnection::update() {
    if (dispatcher.is_empty()) return;
    if (dispatcher.is_active()) return;

    bool established = true;

    for (auto& [node_id, connection] : connections) {
        ConnectionState state = connection.get_state();

        if (state != ConnectionState::ESTABLISHED) {
            established = false;
        }

        if (state == ConnectionState::CLOSED) {
            connection.connect();
        }
    }

    if (!established) return;

    dispatcher.update();
}

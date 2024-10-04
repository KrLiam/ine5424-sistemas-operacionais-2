
#include "communication/broadcast_connection.h"

BroadcastConnection::BroadcastConnection(
    std::map<std::string, Connection>& connections,
    BufferSet<std::string>& connection_update_buffer,
    Pipeline& pipeline
) : connections(connections), pipeline(pipeline), connection_update_buffer(connection_update_buffer)
{
    observe_pipeline();
}


void BroadcastConnection::observe_pipeline() {
    obs_connection_established.on(std::bind(&BroadcastConnection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&BroadcastConnection::connection_closed, this, _1));
    obs_transmission_fail.on(std::bind(&BroadcastConnection::transmission_fail, this, _1));
    obs_transmission_complete.on(std::bind(&BroadcastConnection::transmission_complete, this, _1));
    pipeline.attach(obs_connection_established);
    pipeline.attach(obs_connection_closed);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
}

void BroadcastConnection::connection_established(const ConnectionEstablished&) {
    request_update();
}

void BroadcastConnection::connection_closed(const ConnectionClosed&) {
    cancel_transmissions();
}

void BroadcastConnection::transmission_complete(const TransmissionComplete& event) {
    if (!active_transmission)
        return;

    const UUID &uuid = event.uuid;

    if (uuid != active_transmission->uuid)
        return;

    active_transmission->set_result(true);
    complete_transmission();
}

void BroadcastConnection::transmission_fail(const TransmissionFail& event) {
    if (!active_transmission)
        return;

    Packet &packet = event.faulty_packet;
    const UUID &uuid = packet.meta.transmission_uuid;

    if (uuid != active_transmission->uuid)
        return;

    active_transmission->set_result(false);
    complete_transmission();
}

void BroadcastConnection::complete_transmission() {
    if (!active_transmission)
        return;

    mutex_transmissions.lock();
    int size = transmissions.size();
    for (int i = 0; i < size; i++)
    {
        if (transmissions[i] != active_transmission)
            continue;

        transmissions.erase(transmissions.begin() + i);
        break;
    }
    mutex_transmissions.unlock();

    active_transmission->release();
    active_transmission = nullptr;

    if (transmissions.size())
        request_update();
}

void BroadcastConnection::cancel_transmissions()
{
    if (active_transmission)
    {
        active_transmission->active = false;
        pipeline.notify(PipelineCleanup(active_transmission->message));
        active_transmission = nullptr;
    }

    mutex_transmissions.lock();
    for (Transmission *transmission : transmissions)
    {
        transmission->set_result(false);
        transmission->release();
    }
    transmissions.clear();
    mutex_transmissions.unlock();
}

void BroadcastConnection::request_update() {
    connection_update_buffer.produce(BROADCAST_ID);
}

bool BroadcastConnection::enqueue(Transmission& transmission) {
    mutex_transmissions.lock();
    transmissions.push_back(&transmission);
    mutex_transmissions.unlock();

    request_update();

    return true;
}

void BroadcastConnection::update() {
    if (transmissions.empty()) return;

    if (active_transmission) return;

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

    active_transmission = transmissions.at(0);
    pipeline.send(active_transmission->message);
}

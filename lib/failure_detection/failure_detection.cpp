#include "failure_detection.h"
#include "utils/date.h"

FailureDetection::FailureDetection(std::shared_ptr<GroupRegistry> gr, EventBus &event_bus, unsigned int alive)
    : gr(gr), event_bus(event_bus), alive(alive), keep_alive(alive * MAX_HEARTBEAT_TRIES), running(true)
{
    attach();
    timer_id = TIMER.add(alive, [this]() { failure_detection_routine(); });
}

FailureDetection::~FailureDetection()
{
    terminate();
}

void FailureDetection::terminate() {
    running = false;
    if (timer_id >= 0) TIMER.cancel(timer_id);
}

void FailureDetection::connection_established(const ConnectionEstablished &event)
{
    last_alive.emplace(event.node.get_id(), DateUtils::now());
}

void FailureDetection::connection_closed(const ConnectionClosed &event)
{
    last_alive.erase(event.node.get_id());
}

void FailureDetection::packet_received(const PacketReceived &event)
{
    Packet& packet = event.packet;
    Node& node = gr->get_nodes().get_node(packet.meta.origin);

    log_trace("Received ", packet.to_string(), " (node ", node.get_id(), ").");

    const UUID uuid = UUID(std::string(event.packet.data.header.uuid));

    mtx.lock();
    if (node.is_alive() && uuid != node.get_uuid())
    {
        log_info(
            "Node ",
            node.get_address().to_string(),
            " restarted. Notifying about its death before marking it as alive.");
        node.set_alive(false);
        event_bus.notify(NodeDeath(node));
    }

    last_alive[node.get_id()] = DateUtils::now();

    if (!node.is_alive())
    {
        node.set_uuid(uuid);
        node.set_alive(true);

        log_info(
            "Received ",
            event.packet.to_string(PacketFormat::RECEIVED),
            " (node ",
            node.get_id(),
            "). Marking it as alive."
        );
        // TODO: dá pra lançar um evento NodeAlive aqui caso seja necessário futuramente
    }
    mtx.unlock();

}

void FailureDetection::failure_detection_routine()
{
    if (!running) return;

    for (auto &[id, node] : gr->get_nodes())
    {
        mtx.lock();
        if (node.is_alive() && DateUtils::now() - last_alive[node.get_id()] > keep_alive)
        {
            node.set_alive(false);
            
            log_warn(
                "No heartbeat from node '",
                node.get_id(),
                "' (",
                node.get_address().to_string(),
                ") in the last ",
                keep_alive,
                " milliseconds; marking it as dead."
            );
            event_bus.notify(NodeDeath(node));
        }
        mtx.unlock();

        Connection &conn = gr->get_connection(node);
        conn.heartbeat();
    }

    timer_id = TIMER.add(alive, [this]() { failure_detection_routine(); });
}

void FailureDetection::attach()
{
    obs_connection_established.on(std::bind(&FailureDetection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&FailureDetection::connection_closed, this, _1));
    obs_packet_received.on(std::bind(&FailureDetection::packet_received, this, _1));
    event_bus.attach(obs_connection_established);
    event_bus.attach(obs_connection_closed);
    event_bus.attach(obs_packet_received);
}

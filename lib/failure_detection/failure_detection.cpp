#include "failure_detection.h"
#include "utils/date.h"

FailureDetection::FailureDetection(std::shared_ptr<GroupRegistry> gr, EventBus &event_bus, unsigned int alive)
    : gr(gr), event_bus(event_bus), alive(alive), keep_alive(alive * MAX_HEARTBEAT_TRIES), running(true)
{
    attach();
    failure_detection_thread = std::thread([this]() { failure_detection_routine(); });
}

FailureDetection::~FailureDetection()
{
    running = false;
    if (failure_detection_thread.joinable())
        failure_detection_thread.join();
}

void FailureDetection::connection_established(const ConnectionEstablished &event)
{
    last_alive.emplace(event.node.get_id(), DateUtils::now());
}

void FailureDetection::connection_closed(const ConnectionClosed &event)
{
    last_alive.erase(event.node.get_id());
}

void FailureDetection::heartbeat_received(const HeartbeatReceived &event)
{
    Node& node = event.remote_node;

    log_trace("Received heartbeat from ", node.get_address().to_string(), ".");
    last_alive[node.get_id()] = DateUtils::now();
    node.set_alive(true);

    // TODO: dá pra lançar um evento NodeAlive aqui caso seja necessário futuramente
}

void FailureDetection::failure_detection_routine()
{
    log_info("Initialized failure detection thread.");
    while (running)
    {
        uint64_t now = DateUtils::now();

        for (auto &[id, node] : gr->get_nodes())
        {
            if (node.is_alive() && now - last_alive[node.get_id()] > keep_alive)
            {
                log_warn("No heartbeat from ",
                         node.get_address().to_string(),
                         " in the last ",
                         keep_alive,
                         " milliseconds; marking node as dead.");
                node.set_alive(false);
                event_bus.notify(NodeDeath(node));
            }

            Connection &conn = gr->get_connection(node);
            conn.heartbeat();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(alive));
    }
    log_info("Closed failure detection thread.");
}

void FailureDetection::attach()
{
    obs_connection_established.on(std::bind(&FailureDetection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&FailureDetection::connection_closed, this, _1));
    obs_heartbeat_received.on(std::bind(&FailureDetection::heartbeat_received, this, _1));
    event_bus.attach(obs_connection_established);
    event_bus.attach(obs_connection_closed);
    event_bus.attach(obs_heartbeat_received);
}

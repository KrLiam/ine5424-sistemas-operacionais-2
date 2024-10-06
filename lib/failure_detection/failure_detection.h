#pragma once

#include <chrono>
#include <functional>
#include <thread>
#include <map>

#include "communication/group_registry.h"
#include "core/event.h"
#include "core/event_bus.h"
#include "utils/log.h"
#include "utils/observer.h"

class FailureDetection
{
    unsigned int alive;
    unsigned int keep_alive;

    std::map<std::string, uint64_t> last_alive;

    GroupRegistry *gr; // TODO: usar shared_ptr

    EventBus &event_bus;

    std::thread failure_detection_thread;

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<HeartbeatReceived> obs_heartbeat_received;

    void connection_established(const ConnectionEstablished &event)
    {
        last_alive.emplace(event.node.get_id(), DateUtils::now());
    }

    void connection_closed(const ConnectionClosed &event)
    {
        last_alive.erase(event.node.get_id());
    }

    void heartbeat_received(const HeartbeatReceived &event)
    {
        log_trace("Received heartbeat from ", event.remote_node.get_address().to_string(), ".");
        last_alive[event.remote_node.get_id()] = DateUtils::now();
    }

    void failure_detection_routine()
    {
        log_info("Initialized failure detection thread.");
        while (true)
        {
            uint64_t now = DateUtils::now();

            for (auto &[id, node] : gr->get_nodes())
            {
                Connection &conn = gr->get_connection(node);
                if (conn.get_state() != ConnectionState::ESTABLISHED)
                    continue;

                if (now - last_alive[node.get_id()] > keep_alive)
                {
                    log_warn("No heartbeat from ",
                             node.get_address().to_string(),
                             " in the last ",
                             keep_alive,
                             " milliseconds; marking node as dead.");
                    event_bus.notify(NodeDeath(node));
                    continue;
                }

                conn.heartbeat();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(alive));
        }
        log_info("Closed failure detection thread.");
    }

public:
    FailureDetection(GroupRegistry *gr, EventBus &event_bus, unsigned int alive) : gr(gr), event_bus(event_bus), alive(alive), keep_alive(alive * 5)
    {
        attach();

        failure_detection_thread = std::thread([this]()
                                               { failure_detection_routine(); });
    }

    void attach()
    {
        obs_connection_established.on(std::bind(&FailureDetection::connection_established, this, _1));
        obs_connection_closed.on(std::bind(&FailureDetection::connection_closed, this, _1));
        obs_heartbeat_received.on(std::bind(&FailureDetection::heartbeat_received, this, _1));
        event_bus.attach(obs_connection_established);
        event_bus.attach(obs_connection_closed);
        event_bus.attach(obs_heartbeat_received);
    }
};
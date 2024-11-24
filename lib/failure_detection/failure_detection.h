#pragma once

#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "communication/group_registry.h"
#include "core/event.h"
#include "core/event_bus.h"
#include "utils/log.h"
#include "utils/observer.h"

class FailureDetection
{
    static const int ALIVE_TOLERANCE = 6;

    std::unordered_map<std::string, uint64_t> last_alive;
    std::mutex mtx;

    std::unordered_map<std::string, int> hb_timers;
    std::mutex hb_timers_mtx;

    std::unordered_map<std::string, std::unordered_set<SocketAddress>> suspicion_map;

    std::shared_ptr<GroupRegistry> gr;

    EventBus &event_bus;

    unsigned int alive;
    int timer_id = -1;

    bool running;

    void process_heartbeat_data(const Packet& packet);

    std::unordered_set<SocketAddress> &get_suspicions(std::string node_id);
    std::unordered_map<SocketAddress, unsigned int> count_suspicions();

    void check_for_faulty_nodes();

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<PacketReceived> obs_packet_received;
    Observer<PacketSent> obs_packet_sent;

    void connection_established(const ConnectionEstablished &event);
    void connection_closed(const ConnectionClosed &event);
    void packet_received(const PacketReceived &event);
    void packet_sent(const PacketSent &event);

    void failure_detection_routine();

    void schedule_heartbeat(const Node& node);

public:
    FailureDetection(std::shared_ptr<GroupRegistry> gr, EventBus &event_bus, unsigned int alive);

    ~FailureDetection();

    void terminate();

    void attach();

    void heartbeat(const Node& node);
};

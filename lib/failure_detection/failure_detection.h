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
    std::shared_ptr<GroupRegistry> gr;
    EventBus &event_bus;
    unsigned int alive;
    bool verbose = false;

    static const int ALIVE_TOLERANCE = 6;

    std::unordered_map<std::string, uint64_t> last_alive;
    std::unordered_map<std::string, std::unordered_set<SocketAddress>> suspicion_map;
    std::unordered_map<std::string, int> hb_timers;
    bool running = true;
    int timer_id = -1;

    std::mutex hb_timers_mtx;
    std::mutex mtx;

    std::unordered_set<SocketAddress> &get_suspicions(std::string node_id);
    std::unordered_map<SocketAddress, unsigned int> count_suspicions();

    void check_for_faulty_nodes();

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<PacketReceived> obs_packet_received;
    Observer<PacketSent> obs_packet_sent;
    Observer<NodeUp> obs_node_up;

    void connection_established(const ConnectionEstablished &event);
    void connection_closed(const ConnectionClosed &event);
    void packet_received(const PacketReceived &event);
    void packet_sent(const PacketSent &event);
    void node_up(const NodeUp &event);

    void failure_detection_routine();

    void heartbeat(const Node& node);
    void process_heartbeat(const Packet& packet);
    void schedule_heartbeat(const Node& node);

    void send_discover(const Node& node);
    void process_discover(Packet& packet);
    void discover(const SocketAddress& address);
    void send_discover_ack(const Packet& packet);

public:
    FailureDetection(
        std::shared_ptr<GroupRegistry> gr,
        EventBus &event_bus,
        unsigned int alive,
        bool verbose
    );

    ~FailureDetection();

    void terminate();

    void attach();
};

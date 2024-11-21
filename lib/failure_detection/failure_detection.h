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

    std::unordered_map<std::string, uint64_t> last_alive;
    std::unordered_map<std::string, uint64_t> last_sent;
    std::mutex mtx;

    std::shared_ptr<GroupRegistry> gr;

    EventBus &event_bus;

    unsigned int alive;
    unsigned int keep_alive;
    int timer_id = -1;

    bool running;

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<PacketReceived> obs_packet_received;
    Observer<PacketSent> obs_packet_sent;

    void connection_established(const ConnectionEstablished &event);
    void connection_closed(const ConnectionClosed &event);
    void packet_received(const PacketReceived &event);
    void packet_sent(const PacketSent &event);

    void failure_detection_routine();

public:
    FailureDetection(std::shared_ptr<GroupRegistry> gr, EventBus &event_bus, unsigned int alive);

    ~FailureDetection();

    void terminate();

    void attach();
};

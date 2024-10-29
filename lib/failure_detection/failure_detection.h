#pragma once

#include <chrono>
#include <functional>
#include <thread>
#include <map>
#include <memory>
#include <mutex>
#include "communication/group_registry.h"
#include "core/event.h"
#include "core/event_bus.h"
#include "utils/log.h"
#include "utils/observer.h"

class FailureDetection
{
    UUID uuid;

    std::map<std::string, uint64_t> last_alive;

    std::shared_ptr<GroupRegistry> gr;

    EventBus &event_bus;

    unsigned int alive;
    unsigned int keep_alive;

    bool running;

    std::mutex mtx;

    std::thread failure_detection_thread;

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<HeartbeatReceived> obs_heartbeat_received;

    void connection_established(const ConnectionEstablished &event);

    void connection_closed(const ConnectionClosed &event);

    void heartbeat_received(const HeartbeatReceived &event);

    void failure_detection_routine();

public:
    FailureDetection(std::shared_ptr<GroupRegistry> gr, EventBus &event_bus, unsigned int alive);

    ~FailureDetection();

    void attach();
};

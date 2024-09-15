#pragma once

#include <chrono>
#include <thread>
#include <functional>
#include <condition_variable>
#include <memory>

#include "utils/log.h"

class DateUtils
{
public:
    static uint64_t now();
};


class TimerEntry {
    int id;
    int interval_ms;
    std::binary_semaphore* expire_sem;
    std::function<void()> callback;

    bool cancelled = false;
    bool expired = false;
    std::mutex mutex;
    std::condition_variable var;
    std::thread thread;

    void wait();

public:
    TimerEntry(
        std::binary_semaphore* expire_sem,
        int id,
        uint64_t interval_ms,
        std::function<void()> callback
    );
    
    ~TimerEntry();

    void cancel();

    int get_id();

    bool is_expired();
};


class Timer {
    std::vector<std::shared_ptr<TimerEntry>> timers;
    std::mutex timers_mutex;

    std::binary_semaphore expire_sem{0};
    std::thread thread;
    bool stop = false;
    int current_id = 0;

    void clear_expired_timers();
public:
    Timer();

    ~Timer();

    int add(int interval_ms, std::function<void()> callback);

    bool cancel(int id);
};

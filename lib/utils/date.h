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


struct TimerEntry {
    int id;
    uint64_t date;
    std::function<void()> callback;
    bool cancelled = false;

    TimerEntry(
        int id,
        uint64_t date,
        std::function<void()> callback
    );
};


class Timer {
    std::vector<std::shared_ptr<TimerEntry>> timers;
    std::mutex timers_mutex;

    std::binary_semaphore has_timers_sem{0};
    std::mutex mutex;
    std::condition_variable var;

    std::thread thread;
    bool stop = false;
    int current_id = 0;

    void routine();
public:
    Timer();

    ~Timer();

    int add(int interval_ms, std::function<void()> callback);

    bool cancel(int id);
};

extern Timer TIMER;

#include "utils/date.h"


uint64_t DateUtils::now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


TimerEntry::TimerEntry(
    int id,
    uint64_t date,
    std::function<void()> callback
) : id(id), date(date), callback(callback) {}


Timer::Timer() {
    thread = std::thread([this]() { routine(); });
}

Timer::~Timer() {
    stop = true;
    var.notify_all();
    has_timers_sem.release();

    thread.join();
}

int Timer::add(int interval_ms, std::function<void()> callback) {
    uint64_t now = DateUtils::now();
    uint64_t date = now + interval_ms;

    current_id++;
    auto timer = std::make_shared<TimerEntry>(current_id, date, callback);

    timers_mutex.lock();

    size_t insert_i = timers.size();
    for (int i=timers.size() - 1; i >= 0; i--) {
        auto& t = timers.at(i);
        if (t->date < date) break;

        insert_i = i;
    }
    // log_debug("Insert timer ", current_id, " scheduled for ", date % 60000, " at pos ", insert_i);
    timers.insert(timers.begin() + insert_i, timer);

    timers_mutex.unlock();

    var.notify_all();
    has_timers_sem.release();

    return current_id;
}

bool Timer::cancel(int id) {
    bool success = false;

    timers_mutex.lock();

    int size = timers.size();

    for (int i = 0; i < size; i++) {
        auto timer = timers.at(i);

        if (timer->id == id) {
            // log_debug("Cancelled timer ", id);
            timer->cancelled = true;
            var.notify_all();

            success = true;
            break;
        }
    }

    timers_mutex.unlock();

    return success;
}

void Timer::routine() {
    while (!stop) {
        while (!timers.size()) {
            // log_debug("Waiting for active timer");
            has_timers_sem.acquire();
            continue;
        }

        timers_mutex.lock();

        std::shared_ptr<TimerEntry> next_timer = timers.at(0);

        uint64_t timer_date = next_timer->date;
        uint64_t now = DateUtils::now();
        int interval = timer_date - now;

        // log_debug("Next timer is ", next_timer->id, " in ", interval, "ms");

        if (interval <= 0 || next_timer->cancelled) {
            timers.erase(timers.begin());
            
            timers_mutex.unlock();
            
            // log_debug("Trigerring timer ", next_timer->id, ", time is ", now % 60000);
            if (!next_timer->cancelled) next_timer->callback();

            continue;
        }

        timers_mutex.unlock();

        // log_debug("Sleeping for ", interval, "ms");
        std::unique_lock<std::mutex> lock(mutex);
        var.wait_for(lock, std::chrono::milliseconds(interval));
    }
}
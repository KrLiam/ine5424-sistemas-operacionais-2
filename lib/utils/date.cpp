#include "utils/date.h"


uint64_t DateUtils::now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


TimerEntry::TimerEntry(
    int id,
    uint64_t date,
    std::function<void()> callback
) : id(id), date(date), callback(callback) {}


Timer::Timer() {}

void Timer::init() {
    initialized = true;
    thread = std::thread([this]() { routine(); });
}
void Timer::reset() {
    timers_mutex.lock();

    stop = true;
    timers.clear();
    var.notify_all();
    has_timers_sem.release();

    timers_mutex.unlock();

    if (thread.joinable()) thread.join();

}

Timer::~Timer() {
    stop = true;
    var.notify_all();
    has_timers_sem.release();

    if (thread.joinable()) thread.join();
}

int Timer::add(int interval_ms, std::function<void()> callback) {
    if (stop) return false;
    if (!initialized) init();
    
    uint64_t now = DateUtils::now();
    uint64_t date = now + interval_ms;

    timers_mutex.lock();

    int timer_id = ++current_id;
    auto timer = std::make_shared<TimerEntry>(current_id, date, callback);

    size_t insert_i = timers.size();
    for (int i=timers.size() - 1; i >= 0; i--) {
        auto& t = timers.at(i);
        if (t->date < date) break;

        insert_i = i;
    }
    timers.insert(timers.begin() + insert_i, timer);

    timers_mutex.unlock();

    var.notify_all();
    has_timers_sem.release();

    return timer_id;
}

bool Timer::cancel(int id) {
    if (stop) return false;

    bool success = false;

    timers_mutex.lock();

    int size = timers.size();

    for (int i = 0; i < size; i++) {
        auto timer = timers.at(i);
        if (!timer) continue;

        if (timer->id == id) {
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
            has_timers_sem.acquire();

            if (stop) return;
            continue;
        }

        timers_mutex.lock();

        if (!timers.size()) {
            timers_mutex.unlock();
            continue;
        }
        std::shared_ptr<TimerEntry> next_timer = timers.at(0);

        uint64_t timer_date = next_timer->date;
        uint64_t now = DateUtils::now();
        int interval = timer_date - now;

        if (interval <= 0 || next_timer->cancelled) {
            timers.erase(timers.begin());
            
            timers_mutex.unlock();
            
            if (!next_timer->cancelled) next_timer->callback();

            continue;
        }
        else {
            timers_mutex.unlock();
        }


        std::unique_lock<std::mutex> lock(mutex);
        var.wait_for(lock, std::chrono::milliseconds(interval));
    }
}


#include "utils/date.h"


uint64_t DateUtils::now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


void TimerEntry::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    var.wait_for(lock, std::chrono::milliseconds(interval_ms));
    // log_debug("Timer ", id, " timed out. cancelled: ", cancelled);

    if (!cancelled) callback();

    expired = true;
    expire_sem->release();
}

TimerEntry::TimerEntry(
    std::binary_semaphore* expire_sem,
    int id,
    uint64_t interval_ms,
    std::function<void()> callback
) : id(id), interval_ms(interval_ms), expire_sem(expire_sem), callback(callback) {
    // log_debug("Created timer ", id);
    thread = std::thread([this]() { wait(); });
}

TimerEntry::~TimerEntry() {
    // log_debug("Destructing timer ", id);
    if (!cancelled) cancel();
    thread.join();
}

void TimerEntry::cancel() {
    if (cancelled || expired) return;
    
    // log_debug("Cancelled timer ", id);
    cancelled = true;
    var.notify_all();
}

int TimerEntry::get_id() {
    return id;
}

bool TimerEntry::is_expired() {
    return expired;
}


void Timer::clear_expired_timers() {
    while (!stop) {
        expire_sem.acquire();

        timers_mutex.lock();

        int size = timers.size();

        for (int i = size - 1; i >= 0; i--) {
            auto timer = timers.at(i);

            if (timer->is_expired()) {
                // log_debug("Dropping timer ", timer->get_id());
                timers.erase(timers.begin() + i);
            }
        }

        timers_mutex.unlock();
    }
}

Timer::Timer() {
    thread = std::thread([this]() { clear_expired_timers(); });
}

Timer::~Timer() {
    stop = true;
    expire_sem.release();
    thread.join();
}

int Timer::add(int interval_ms, std::function<void()> callback) {
    current_id++;
    auto timer = std::make_shared<TimerEntry>(&expire_sem, current_id, interval_ms, callback);

    timers_mutex.lock();
    timers.push_back(timer);
    timers_mutex.unlock();

    return current_id;
}

bool Timer::cancel(int id) {
    bool success = false;

    timers_mutex.lock();

    int size = timers.size();

    for (int i = 0; i < size; i++) {
        auto timer = timers.at(i);

        if (timer->get_id() == id) {
            timer->cancel();
            success = true;
            break;
        }
    }

    timers_mutex.unlock();

    return success;
}


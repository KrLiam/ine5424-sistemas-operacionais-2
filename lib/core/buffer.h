#pragma once

#include <mutex>
#include <semaphore>
#include <condition_variable>
#include <unordered_set>
#include <queue>

#include "utils/log.h"

class buffer_termination final : public std::runtime_error {
public:
    explicit buffer_termination();
    explicit buffer_termination(const std::string &msg);
};


template <typename T>
class Buffer
{
public:
    Buffer() {};
    Buffer(uint32_t max_size) : max_size(max_size) {};
    Buffer(std::string name) : name(name) {};
    Buffer(std::string name, uint32_t max_size) : name(name), max_size(max_size) {};

    bool empty() { return !buffer.size(); }
    bool full() { return buffer.size() >= max_size; }

    T consume()
    {
        while (empty() && !terminating) {
            log_trace("Waiting to consume on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(empty_mutex);
            empty_cv.wait(lock);
        }

        if (terminating) throw buffer_termination("Exiting buffer.");

        mutex.lock();

        T item = buffer.front();
        buffer.pop();

        full_cv.notify_one();

        log_trace("Consumed item to [", name, "] buffer.");

        mutex.unlock();

        return item;
    };

    void produce(const T &item)
    {
        while (full() && !terminating) {
            log_trace("Waiting to produce on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(full_mutex);
            full_cv.wait(lock);
        }

        if (terminating) throw buffer_termination("Exiting buffer.");

        mutex.lock();

        buffer.push(item);
        empty_cv.notify_one();

        log_trace("Produced item to [", name, "] buffer.");

        mutex.unlock();
    }

    void terminate() {
        terminating = true;
        empty_cv.notify_all();
        full_cv.notify_all();  
    }
    
    bool can_consume()
    {
        return buffer.size() > 0;
    }

    bool can_produce()
    {
        return buffer.size() < max_size;
    }

private:
    std::string name;
    uint32_t max_size = UINT32_MAX;

    std::queue<T> buffer;
    std::mutex mutex;
    bool terminating = false;

    std::mutex empty_mutex;
    std::condition_variable empty_cv;
    std::mutex full_mutex;
    std::condition_variable full_cv;

};


template<typename T>
class BufferSet {
    std::unordered_set<T> values;
    std::mutex values_mutex;

    std::mutex empty_mutex;
    std::condition_variable empty_cv;
    std::mutex full_mutex;
    std::condition_variable full_cv;

    std::string name;
    uint32_t max_size = UINT32_MAX;

    bool terminating = false;
public:
    BufferSet() {};
    BufferSet(uint32_t max_size) : max_size(max_size) {};
    BufferSet(std::string name) : name(name) {};
    BufferSet(std::string name, uint32_t max_size) : name(name), max_size(max_size) {};

    bool empty() { return !values.size(); }
    bool full() { return values.size() >= max_size; }

    void produce(const T& value) {
        while (full() && !terminating) {
            log_trace("Waiting to produce on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(full_mutex);
            full_cv.wait(lock);
        }

        if (terminating) throw buffer_termination("Exiting buffer.");

        values_mutex.lock();

        values.emplace(value);
        empty_cv.notify_one();

        log_trace("Produced item to [", name, "] buffer.");

        values_mutex.unlock();
    }

    T consume() {
        while (empty() && !terminating) {
            log_trace("Waiting to consume on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(empty_mutex);
            empty_cv.wait(lock);
        }

        if (terminating) throw buffer_termination("Exiting buffer.");

        values_mutex.lock();

        T v = *values.begin();
        values.erase(values.begin());
        full_cv.notify_one();

        log_trace("Consumed item to [", name, "] buffer.");

        values_mutex.unlock();

        return v;
    }

    void terminate() {
        terminating = true;
        empty_cv.notify_all();
        full_cv.notify_all();  
    }
};

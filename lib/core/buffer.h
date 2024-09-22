#pragma once

#include <mutex>
#include <semaphore>
#include <condition_variable>
#include <unordered_set>
#include <queue>

#include "utils/log.h"

template <int NUM_OF_ITEMS, typename T>
class Buffer
{
public:
    Buffer(std::string name) : name(name) {};

    T consume()
    {
        consumer.acquire();
        mutex.lock();

        T item = buffer.front();
        buffer.pop();

        mutex.unlock();
        log_trace("Consumed item from [", name, "] buffer.");
        producer.release();

        return item;
    };

    void produce(const T &item)
    {
        producer.acquire();
        mutex.lock();
        buffer.push(item);
        mutex.unlock();
        log_trace("Produced item to [", name, "] buffer.");
        consumer.release();

    }

    bool can_consume()
    {
        return buffer.size() > 0;
    }

    bool can_produce()
    {
        return buffer.size() < NUM_OF_ITEMS;
    }

private:
    std::string name;
    std::queue<T> buffer;
    std::mutex mutex;
    std::counting_semaphore<NUM_OF_ITEMS> consumer{0};
    std::counting_semaphore<NUM_OF_ITEMS> producer{NUM_OF_ITEMS};
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
public:
    BufferSet() {};
    BufferSet(uint32_t max_size) : max_size(max_size) {};
    BufferSet(std::string name) : name(name) {};
    BufferSet(std::string name, uint32_t max_size) : name(name), max_size(max_size) {};

    bool empty() { return !values.size(); }
    bool full() { return values.size() >= max_size; }

    void produce(const T& value) {
        if (full()) {
            log_trace("Waiting to produce on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(full_mutex);
            full_cv.wait(lock, [this] { return !full(); });
        }

        values_mutex.lock();

        values.emplace(value);
        empty_cv.notify_one();

        log_trace("Produced item to [", name, "] buffer.");

        values_mutex.unlock();
    }

    T consume() {
        if (empty()) {
            log_trace("Waiting to consume on [", name, "] buffer.");
            std::unique_lock<std::mutex> lock(empty_mutex);
            empty_cv.wait(lock, [this] { return !empty(); });
        }

        values_mutex.lock();

        T v = *values.begin();
        values.erase(values.begin());
        full_cv.notify_one();

        log_trace("Consumed item to [", name, "] buffer.");

        values_mutex.unlock();

        return v;
    }
};

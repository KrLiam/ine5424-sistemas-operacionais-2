#pragma once

#include <mutex>
#include <semaphore>
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

    void produce(T &item)
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
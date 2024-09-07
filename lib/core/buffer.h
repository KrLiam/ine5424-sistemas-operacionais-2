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
        log_debug("Consumed item from [", name, "] buffer.");
        consumer.release();

        return item;
    };

    bool produce(T &item, bool ensure)
    {
        if (ensure)
        {
            producer.acquire();
        }
        else if (!producer.try_acquire())
        {
            return false;
        }
        mutex.lock();
        buffer.push(item);
        mutex.unlock();
        log_debug("Produced item to [", name, "] buffer.");
        consumer.release();
        return true;
    }

private:
    std::string name;
    std::queue<T> buffer;
    std::mutex mutex;
    std::counting_semaphore<NUM_OF_ITEMS> consumer{0};
    std::counting_semaphore<NUM_OF_ITEMS> producer{NUM_OF_ITEMS};
};
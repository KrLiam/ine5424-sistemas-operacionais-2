#pragma once

#include <vector>
#include <mutex>

#include "core/buffer.h"
#include "pipeline/pipeline.h"
#include "communication/transmission.h"


class TransmissionDispatcher {
    std::string id;
    BufferSet<std::string>& update_buffer;
    Pipeline& pipeline;
    uint32_t max_transmissions;
    bool enumerate_messages;

    std::vector<Transmission*> transmissions;
    Transmission* active_transmission;
    std::mutex mutex_transmissions;
    uint32_t next_number;

    void request_update();

    void activate(Transmission* transmission);
public:
    TransmissionDispatcher(
        std::string id,
        BufferSet<std::string>& update_buffer,
        Pipeline& pipeline
    );
    TransmissionDispatcher(
        std::string id,
        BufferSet<std::string>& update_buffer,
        Pipeline& pipeline,
        bool enumerate_messages
    );

    ~TransmissionDispatcher();

    bool is_empty() const;
    bool is_active() const;

    uint32_t get_next_number() const;
    const Transmission* get_active() const;
    Transmission* get_next();

    bool enqueue(Transmission& transmission);

    void increment_number(); // TODO isso é temporário

    void update();

    void complete(bool success);

    void cancel_all();
    void reset_number();
    void reset_number(uint32_t num);
};

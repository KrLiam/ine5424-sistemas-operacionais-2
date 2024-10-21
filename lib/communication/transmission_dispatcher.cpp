
#include "communication/transmission_dispatcher.h"

TransmissionDispatcher::TransmissionDispatcher(
    std::string id,
    BufferSet<std::string>& update_buffer,
    Pipeline& pipeline
) :
    id(id),
    update_buffer(update_buffer),
    pipeline(pipeline),
    max_transmissions(MAX_ENQUEUED_TRANSMISSIONS),
    active_transmission(nullptr),
    next_number(0) {};

TransmissionDispatcher::~TransmissionDispatcher() {
    cancel_all();
}

bool TransmissionDispatcher::is_empty() const {
    return transmissions.empty();
}

bool TransmissionDispatcher::is_active() const {
    return active_transmission != nullptr;
}

uint32_t TransmissionDispatcher::get_next_number() const { return next_number; }

const Transmission* TransmissionDispatcher::get_active() const { return active_transmission; }

Transmission* TransmissionDispatcher::get_next() {
    for (Transmission *transmission : transmissions) {
        if (!transmission->active) return transmission;
    }

    return nullptr;
}

bool TransmissionDispatcher::enqueue(Transmission& transmission) {
    mutex_transmissions.lock();

    if (transmissions.size() >= MAX_ENQUEUED_TRANSMISSIONS) {
        mutex_transmissions.unlock();
        return false;
    }
    
    transmissions.push_back(&transmission);

    mutex_transmissions.unlock();

    log_debug("Enqueued transmission ", transmission.uuid, " on connection of node ", transmission.receiver_id);

    request_update();
    return true;
}

void TransmissionDispatcher::increment_number() {
    next_number++;
}

void TransmissionDispatcher::request_update() {
    update_buffer.produce(id);
}

void TransmissionDispatcher::update() {
    if (active_transmission) return;
    if (is_empty()) return;

    active_transmission = get_next();
    activate(active_transmission);
}
void TransmissionDispatcher::activate(Transmission* transmission) {
    if (transmission->active) return;

    transmission->active = true;

    Message& message = transmission->message;
    message.id.msg_num = next_number++;

    pipeline.notify(TransmissionStarted(message));
    pipeline.send(message);
}

void TransmissionDispatcher::complete(bool success) {
    if (!active_transmission) return;

    active_transmission->set_result(success);

    int size = transmissions.size();
    for (int i = 0; i < size; i++) {
        if (transmissions[i] != active_transmission)
            continue;

        transmissions.erase(transmissions.begin() + i);
        break;
    }

    active_transmission->release();
    active_transmission = nullptr;

    if (transmissions.size()) request_update();
}

void TransmissionDispatcher::cancel_all() {
    if (active_transmission) {
        active_transmission->active = false;
        pipeline.notify(PipelineCleanup(active_transmission->message));
        active_transmission = nullptr;
    }

    mutex_transmissions.lock();
    for (Transmission *transmission : transmissions) {
        transmission->set_result(false);
        transmission->release();
    }
    transmissions.clear();
    mutex_transmissions.unlock();
}

void TransmissionDispatcher::reset_number() {
    reset_number(0);
}
void TransmissionDispatcher::reset_number(uint32_t num) {
    next_number = num;
}
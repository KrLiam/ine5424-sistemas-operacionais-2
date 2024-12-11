
#include <semaphore>
#include <mutex>

#include "communication/transmission.h"

Transmission::Transmission(Message m, std::string receiver_id)
    : uuid(UUID()),
      receiver_id(receiver_id),
      message(m)
{
    message.transmission_uuid = uuid;
}

bool Transmission::is_broadcast() const {
    return receiver_id == BROADCAST_ID;
}

bool Transmission::to_leader() const {
    return receiver_id == LEADER_ID;
}

void Transmission::set_result(bool success) {
    result.success = success;
}

void Transmission::release() {
    active = false;
    completed = true;
    completed_sem.release();
} 

TransmissionResult Transmission::wait_result() {
    if (!completed) completed_sem.acquire();
    return result;
}

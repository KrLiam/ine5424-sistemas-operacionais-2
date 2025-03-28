
#include <semaphore>
#include <mutex>

#include "communication/transmission.h"

Transmission::Transmission(std::string receiver_id, Message m)
    : uuid(UUID()),
      receiver_id(receiver_id),
      message(m)
{
    message.transmission_uuid = uuid;
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
    // trocar pra condition_variable
    if (!completed) completed_sem.acquire();
    return result;
}

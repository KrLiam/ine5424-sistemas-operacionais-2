#pragma once

#include <semaphore>

#include "core/message.h"

struct TransmissionResult {
    bool success;
    // ...
};


class Transmission {
public:
    /**
     * Identificar global da transmissão. Cada envio de mensagem é associado
     * a um identificador de transmissão único.
    */
    const UUID uuid;
    std::string receiver_id;
    Message message;
    /**
     * Semáforo que é liberado somente quando esta transmissão finalizar, seja
     * com falha ou não.
    */
    std::binary_semaphore completed_sem{0};
    bool active = false;
    bool completed = false;
    TransmissionResult result;

    Transmission(std::string receiver_id, Message message);

    void set_result(bool success);

    void release();

    TransmissionResult wait_result();
};
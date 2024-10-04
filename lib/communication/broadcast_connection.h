#pragma once

#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "communication/transmission.h"

class BroadcastConnection {
    std::map<std::string, Connection>& connections;
    Pipeline& pipeline;

    Transmission* active_transmission = nullptr;
    std::vector<Transmission*> transmissions;
    std::mutex mutex_transmissions;

    BufferSet<std::string>& connection_update_buffer;

    void cancel_transmissions();
    void complete_transmission();

    void observe_pipeline();

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<TransmissionComplete> obs_transmission_complete;
    Observer<TransmissionFail> obs_transmission_fail;

    void connection_established(const ConnectionEstablished& event);
    void connection_closed(const ConnectionClosed& event);
    void transmission_complete(const TransmissionComplete& event);
    void transmission_fail(const TransmissionFail& event);

public:
    BroadcastConnection(
        std::map<std::string, Connection>& connections,
        BufferSet<std::string>& connection_update_buffer,
        Pipeline& pipeline
    );

    bool enqueue(Transmission& transmission);

    void request_update();
    void update();
};

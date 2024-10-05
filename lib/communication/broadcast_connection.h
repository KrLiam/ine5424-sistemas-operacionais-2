#pragma once

#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "communication/transmission.h"

class BroadcastConnection {
    std::map<std::string, Connection>& connections;
    Pipeline& pipeline;

    TransmissionDispatcher dispatcher;
    BufferSet<std::string>& connection_update_buffer;

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

    const TransmissionDispatcher& get_dispatcher() const;

    bool enqueue(Transmission& transmission);

    void request_update();
    void update();
};

#pragma once

#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "communication/transmission.h"

class BroadcastConnection {
    std::map<std::string, Connection>& connections;
    Pipeline& pipeline;

    TransmissionDispatcher dispatcher;
    BufferSet<std::string>& connection_update_buffer;
    Buffer<Message>& deliver_buffer;

    void observe_pipeline();

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<TransmissionComplete> obs_transmission_complete;
    Observer<TransmissionFail> obs_transmission_fail;
    Observer<MessageReceived> obs_message_received;
    Observer<FragmentReceived> obs_fragment_received;
    Observer<PacketAckReceived> obs_packet_ack_received;

    void connection_established(const ConnectionEstablished& event);
    void connection_closed(const ConnectionClosed& event);
    void transmission_complete(const TransmissionComplete& event);
    void transmission_fail(const TransmissionFail& event);
    void message_received(const MessageReceived &event);
    void fragment_received(const FragmentReceived &event);
    void packet_ack_received(const PacketAckReceived &event);

public:
    BroadcastConnection(
        std::map<std::string, Connection>& connections,
        BufferSet<std::string>& connection_update_buffer,
        Buffer<Message>& deliver_buffer,
        Pipeline& pipeline
    );

    const TransmissionDispatcher& get_dispatcher() const;

    bool enqueue(Transmission& transmission);

    void request_update();
    void update();
};

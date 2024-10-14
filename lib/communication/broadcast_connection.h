#pragma once

#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "communication/transmission.h"

struct RetransmissionEntry {
    UUID uuid;
    Message message;
    std::unordered_set<uint32_t> retransmitted_fragments;
    std::unordered_map<uint32_t, std::unordered_set<SocketAddress>> received_acks;
    bool message_received = false;
    bool received_all_acks = false;

    RetransmissionEntry();
};

struct SequenceNumber {
    uint32_t initial_number;
    uint32_t next_number;
};


class BroadcastConnection {
    NodeMap& nodes;
    const Node& local_node;
    std::map<std::string, Connection>& connections;
    Pipeline& pipeline;

    TransmissionDispatcher dispatcher;
    std::unordered_map<std::string, SequenceNumber> sequence_numbers;
    std::unordered_map<MessageIdentity, RetransmissionEntry> retransmissions;
    BufferSet<std::string>& connection_update_buffer;
    Buffer<Message>& deliver_buffer;

    void observe_pipeline();

    void receive_ack(Packet& ack_packet);
    void receive_fragment(Packet& packet);

    void retransmit_fragment(Packet& packet);
    void try_deliver(const MessageIdentity&);
    bool is_delivered(const MessageIdentity&);

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ReceiveSynchronization> obs_receive_synchronization;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<TransmissionComplete> obs_transmission_complete;
    Observer<TransmissionFail> obs_transmission_fail;
    Observer<MessageReceived> obs_message_received;
    Observer<PacketReceived> obs_packet_received;

    void receive_synchronization(const ReceiveSynchronization& event);
    void connection_established(const ConnectionEstablished& event);
    void connection_closed(const ConnectionClosed& event);
    void transmission_complete(const TransmissionComplete& event);
    void transmission_fail(const TransmissionFail& event);
    void message_received(const MessageReceived &event);
    void packet_received(const PacketReceived &event);

    void send_rst(Packet&);

public:
    BroadcastConnection(
        NodeMap& nodes,
        const Node& local_node,
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

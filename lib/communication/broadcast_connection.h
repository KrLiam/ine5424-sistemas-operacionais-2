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
    Node& local_node;
    std::map<std::string, Connection>& connections;
    Pipeline& pipeline;

    std::unordered_map<MessageIdentity, MessageIdentity> atomic_to_request_map;
    std::unordered_map<MessageIdentity, std::shared_ptr<Transmission>> ab_transmissions;
    TransmissionDispatcher ab_dispatcher;
    SequenceNumber ab_sequence_number;
    uint32_t ab_next_deliver;
    uint32_t delayed_ab_number;

    TransmissionDispatcher dispatcher;
    std::unordered_map<std::string, SequenceNumber> sequence_numbers;
    std::unordered_map<MessageIdentity, RetransmissionEntry> retransmissions;
    BufferSet<std::string>& connection_update_buffer;
    Buffer<Message>& deliver_buffer;

    std::vector<Packet> dispatched_packets;
    std::mutex mutex_dispatched_packets;

    void observe_pipeline();

    SequenceNumber* get_sequence(const MessageIdentity& id);

    void receive_ack(Packet& ack_packet);
    void receive_fragment(Packet& packet);

    void retransmit_fragment(Packet& packet);
    void try_deliver(const MessageIdentity&);
    void try_deliver_next_atomic();
    bool is_delivered(const MessageIdentity& id, bool is_atomic);
    bool establish_all_connections();

    Observer<ConnectionEstablished> obs_connection_established;
    Observer<ReceiveSynchronization> obs_receive_synchronization;
    Observer<ConnectionClosed> obs_connection_closed;
    Observer<TransmissionComplete> obs_transmission_complete;
    Observer<TransmissionFail> obs_transmission_fail;
    Observer<MessageReceived> obs_message_received;
    Observer<UnicastMessageReceived> obs_unicast_message_received;
    Observer<PacketReceived> obs_packet_received;
    Observer<NodeDeath> obs_node_death;
    Observer<AtomicMapping> obs_atomic_mapping;

    void receive_synchronization(const ReceiveSynchronization& event);
    void connection_established(const ConnectionEstablished& event);
    void connection_closed(const ConnectionClosed& event);
    void transmission_complete(const TransmissionComplete& event);
    void transmission_fail(const TransmissionFail& event);
    void message_received(const MessageReceived &event);
    void unicast_message_received(const UnicastMessageReceived &event);
    void packet_received(const PacketReceived &event);
    void node_death(const NodeDeath &event);
    void atomic_mapping(const AtomicMapping &event);

    void send_rst(Packet&);

    void send_dispatched_packets();

    void synchronize_ab_number(uint32_t number);
    bool has_pending_ab_delivery();

public:
    BroadcastConnection(
        NodeMap& nodes,
        Node& local_node,
        std::map<std::string, Connection>& connections,
        BufferSet<std::string>& connection_update_buffer,
        Buffer<Message>& deliver_buffer,
        Pipeline& pipeline
    );

    const TransmissionDispatcher& get_dispatcher() const;
    const TransmissionDispatcher& get_ab_dispatcher() const;

    bool enqueue(Transmission& transmission);

    void request_update();
    void update();

    void dispatch_to_sender(Packet);
};

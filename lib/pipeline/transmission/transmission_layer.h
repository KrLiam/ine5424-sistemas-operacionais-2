#pragma once

#include <map>
#include <thread>
#include <atomic>

#include "channels/channel.h"
#include "pipeline/pipeline_step.h"
#include "pipeline/transmission/transmission_queue.h"
#include "core/node.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/buffer.h"

class TransmissionLayer : public PipelineStep
{
private:
    Channel *channel;

    std::map<std::string, TransmissionQueue *> queue_map;

    bool process_ack_field_of_received_packet(Packet packet);

    std::thread sender_thread_obj;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Packet> send_buffer{"send buffer"};

    Observer<PacketAckReceived> obs_ack_received;
    void ack_received(PacketAckReceived event);

public:
    std::atomic<bool> stop_threads; // TODO
    TransmissionLayer(PipelineHandler handler, GroupRegistry *gr, Channel *channel);
    ~TransmissionLayer() override;

    void service();

    void attach(EventBus&);

    void sender_thread();
    void receiver_thread();

    void send(Packet packet);
    void receive(Packet packet);
};

#pragma once

#include <map>
#include <thread>
#include <atomic>

#include "channels/channel.h"
#include "communication/pipeline_step.h"
#include "communication/transmission_queue.h"
#include "core/node.h"
#include "core/segment.h"
#include "core/buffer.h"

class TransmissionLayer : public PipelineStep
{
private:
    Channel *channel;

    std::map<std::string, TransmissionQueue *> queue_map;

    bool process_ack_field_of_received_packet(Packet packet);

    std::thread listener_thread_obj;
    std::thread sender_thread_obj;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Packet> send_buffer{"send buffer"};

public:
    std::atomic<bool> stop_threads; // TODO
    TransmissionLayer(PipelineHandler handler, GroupRegistry *gr, Channel *channel);
    ~TransmissionLayer() override;

    void service();

    void sender_thread();

    void receiver_thread();

    void send(Packet packet);

    void receive(Packet packet);

    void stop_transmission(Packet packet)
    {
        Node origin = gr->get_node(packet.meta.origin);
        if (queue_map.contains(origin.get_id()))
        {
            queue_map.at(origin.get_id())->mark_packet_as_acked(packet);
        }
    }
};

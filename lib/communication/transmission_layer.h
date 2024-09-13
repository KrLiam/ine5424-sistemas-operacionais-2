#pragma once

#include <map>
#include <thread>
#include <atomic>

#include "channels/channel.h"
#include "communication/pipeline_step.h"
#include "communication/transmission_queue.h"
#include "core/node.h"
#include "core/segment.h"

class TransmissionLayer : public PipelineStep
{
private:
    Channel *channel;

    std::map<std::string, TransmissionQueue *> queue_map;

    bool process_ack_field_of_received_packet(Packet packet);
    Packet create_ack_packet(Packet packet);

    std::thread listener_thread_obj;
    std::thread sender_thread_obj;

public:
    std::atomic<bool> stop_threads; // TODO
    TransmissionLayer(PipelineHandler handler, GroupRegistry &gr, Channel *channel);
    ~TransmissionLayer() override;

    void service();

    void sender_thread();

    void receiver_thread();

    void send(char *m);

    void receive(char *m);
};

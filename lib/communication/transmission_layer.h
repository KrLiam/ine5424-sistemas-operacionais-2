#pragma once

#include <map>
#include <thread>

#include "channels/channel.h"
#include "communication/pipeline_step.h"
#include "communication/transmission_queue.h"
#include "core/node.h"
#include "core/segment.h"

class TransmissionLayer : public PipelineStep
{
private:
    Channel *channel;

    std::map<std::string, TransmissionQueue> queue_map;

    void process_ack_of_received_packet(Packet packet);

public:
    TransmissionLayer(PipelineHandler& handler, GroupRegistry& gr, Channel *channel);

    void service();

    void sender_thread();

    void receiver_thread();

    void send(char *m);

    void receive(char *m);
};

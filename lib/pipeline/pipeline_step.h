#pragma once

#include "pipeline/pipeline_handler.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/constants.h"
#include "core/buffer.h"
#include "core/event_bus.h"

class ReliableCommunication;

class PipelineStep
{
protected:
    PipelineHandler handler;
public:
    PipelineStep(PipelineHandler &handler);

    virtual ~PipelineStep();

    virtual void attach(EventBus&);

    virtual void send(Packet packet);
    virtual void send(Message);

    virtual void receive(Packet);
    virtual void receive(Message);
};

#pragma once

#include "channels/channel.h"
#include "communication/group_registry.h"
#include "communication/pipeline_handler.h"
#include "core/segment.h"
#include "core/constants.h"
#include "core/buffer.h"

class ReliableCommunication;

class PipelineStep
{
protected:
    PipelineHandler handler;
    GroupRegistry *gr;
public:
    PipelineStep(PipelineHandler &handler, GroupRegistry *gr);

    virtual ~PipelineStep();

    virtual void service() = 0;

    virtual void send(Packet packet);
    virtual void send(Message);

    virtual void receive(Packet);
    virtual void receive(Message);
};

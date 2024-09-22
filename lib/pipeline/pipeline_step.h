#pragma once

#include "channels/channel.h"
#include "pipeline/pipeline_handler.h"
#include "communication/group_registry.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/constants.h"
#include "core/buffer.h"
#include "core/event_bus.h"

class ReliableCommunication;
class GroupRegistry;

class PipelineStep
{
protected:
    PipelineHandler handler;
    GroupRegistry *gr;
public:
    PipelineStep(PipelineHandler &handler, GroupRegistry *gr);

    virtual ~PipelineStep();

    virtual void attach(EventBus&);

    virtual void send(Packet packet);
    virtual void send(Message);

    virtual void receive(Packet);
    virtual void receive(Message);
};

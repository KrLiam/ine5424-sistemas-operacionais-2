#pragma once

#include <functional>

#include "core/message.h"
#include "core/packet.h"
#include "core/event_bus.h"

class Pipeline;

class PipelineHandler
{
private:
    Pipeline& pipeline;
    EventBus& event_bus;

    int step_index;

    friend Pipeline;

    /**
     * Método para facilmente criar pipeline handlers para cada step.
    */
    PipelineHandler at_index(int step_index) {
        return PipelineHandler(pipeline, event_bus, step_index);
    }

public:
    PipelineHandler(Pipeline& pipeline, EventBus& event_bus, int step_index);

    void forward_send(Packet);
    void forward_send(Message);

    void forward_receive(Packet);
    void forward_receive(Message);

    template <typename T>
    void notify(const T& event) {
        event_bus.notify(event);
    }
};

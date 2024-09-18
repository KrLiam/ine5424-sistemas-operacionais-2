#pragma once

#include <functional>

#include "core/segment.h"

class Pipeline;

class PipelineHandler
{
private:
    Pipeline& pipeline;
    int step_index;

    friend Pipeline;

    /**
     * Método para facilmente criar pipeline handlers para cada step.
    */
    PipelineHandler at_index(int step_index) {
        return PipelineHandler(pipeline, step_index);
    }

public:
    PipelineHandler(Pipeline& pipeline, int step_index);

    void forward_send(Packet);
    void forward_send(Message);

    void forward_receive(Packet);
    void forward_receive(Message);
};

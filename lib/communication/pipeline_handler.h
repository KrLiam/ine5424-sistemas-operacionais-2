#pragma once

#include <functional>

#include "core/segment.h"

class Pipeline;

class PipelineHandler
{
private:
    Pipeline& pipeline;

public:
    PipelineHandler(Pipeline& pipeline);

    void send_to(unsigned int layer_number, char *m);
    void receive_on(unsigned int layer_number, char *m);
    void forward_to_application(Message message);
};

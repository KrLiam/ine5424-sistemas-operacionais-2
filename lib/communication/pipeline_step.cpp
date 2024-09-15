#include "communication/pipeline_step.h"

PipelineStep::PipelineStep(PipelineHandler &handler, GroupRegistry *gr) : handler(handler), gr(gr)
{
}

PipelineStep::~PipelineStep()
{
}

void PipelineStep::send(Message message)
{
    handler.forward_send(message);
}
void PipelineStep::send(Packet packet)
{
    handler.forward_send(packet);
}

void PipelineStep::receive(Message message)
{
    handler.forward_receive(message);
}
void PipelineStep::receive(Packet packet)
{
    handler.forward_receive(packet);
}

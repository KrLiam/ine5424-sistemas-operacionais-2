#include "communication/pipeline_step.h"

PipelineStep::PipelineStep(PipelineHandler &handler, GroupRegistry *gr) : handler(handler), gr(gr)
{
}

PipelineStep::~PipelineStep()
{
}

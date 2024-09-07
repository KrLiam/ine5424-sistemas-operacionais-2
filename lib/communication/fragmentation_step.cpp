#include "communication/fragmentation_step.h"

FragmentationStep::FragmentationStep(StepControl control) : PipelineStep(control) {}

void FragmentationStep::send(Segment &segment)
{
}

void FragmentationStep::receive(Segment &segment)
{
}

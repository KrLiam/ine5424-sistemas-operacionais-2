#pragma once

#include <cmath>
#include <map>

#include "communication/fragment_assembler.h"
#include "communication/pipeline_step.h"

class FragmentationLayer : public PipelineStep
{
private:
    std::map<std::string, FragmentAssembler> assembler_map;

public:
    FragmentationLayer(PipelineHandler& handler, GroupRegistry& gr);

    void service();

    void send(char*m);

    void receive(char*m);
};

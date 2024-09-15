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
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void service();

    void send(Message);

    void receive(Packet);
};

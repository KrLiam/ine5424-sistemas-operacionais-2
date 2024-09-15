#pragma once

#include <cmath>
#include <map>
#include <mutex>

#include "communication/fragment_assembler.h"
#include "communication/pipeline_step.h"

class FragmentationLayer : public PipelineStep
{
private:
    std::map<std::string, FragmentAssembler> assembler_map;
    std::mutex assembler_map_mutex;

    FragmentAssembler& get_assembler(const std::string& id);
    void discard_assembler(const std::string& id);

public:
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void service();

    void send(Message);

    void receive(Packet);
};

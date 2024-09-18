#pragma once

#include <cmath>
#include <map>

#include "pipeline/fragmentation/fragment_assembler.h"
#include "pipeline/pipeline_step.h"

class FragmentationLayer : public PipelineStep
{
private:
    std::map<std::string, FragmentAssembler> assembler_map;

public:
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void service();

    void send(Message);
    void send(Packet);

    void receive(Packet);

    bool is_message_complete(std::string node_id)
    {
        if (!assembler_map.contains(node_id))
            return false;
        FragmentAssembler &assembler = assembler_map.at(node_id);
        return assembler.has_received_all_packets();
    }
    Message assemble_message(std::string node_id)
    {
        FragmentAssembler &assembler = assembler_map.at(node_id);
        Message message = assembler.assemble();
        assembler_map.erase(node_id);
        return message;
    }
};

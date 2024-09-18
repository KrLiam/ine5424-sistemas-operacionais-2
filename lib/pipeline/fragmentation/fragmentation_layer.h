#pragma once

#include <map>

#include "pipeline/fragmentation/fragment_assembler.h"
#include "pipeline/pipeline_step.h"

class FragmentationLayer final : public PipelineStep
{
    std::map<std::string, FragmentAssembler> assembler_map;

public:
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void service() override;

    void send(Message) override;
    void send(Packet) override;

    void receive(Packet) override;

    bool is_message_complete(std::string node_id)
    {
        if (!assembler_map.contains(node_id))
            return false;
        FragmentAssembler &assembler = assembler_map.at(node_id);
        return assembler.has_received_all_packets();
    }
    Message assemble_message(const std::string& node_id)
    {
        FragmentAssembler &assembler = assembler_map.at(node_id);
        Message const message = assembler.assemble();
        assembler_map.erase(node_id);
        return message;
    }
};

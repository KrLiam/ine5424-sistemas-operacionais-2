#pragma once

#include <map>

#include "pipeline/fragmentation/fragment_assembler.h"
#include "pipeline/pipeline_step.h"

class FragmentationLayer final : public PipelineStep
{
    std::map<std::string, FragmentAssembler> assembler_map;

    std::string get_message_identifier(Packet p)
    {
        return format("%s/%s", gr->get_node(p.meta.origin).get_address().to_string().c_str(), std::to_string(p.data.header.get_message_number()).c_str());
    }

public:
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void service() override;

    void send(Message) override;
    void send(Packet) override;

    void receive(Packet) override;

    bool is_message_complete(Packet p)
    {
        std::string id = get_message_identifier(p);
        if (!assembler_map.contains(id))
            return false;
        FragmentAssembler &assembler = assembler_map.at(id);
        return assembler.has_received_all_packets();
    }
    Message assemble_message(Packet p)
    {
        std::string id = get_message_identifier(p);
        Message const message = assembler_map.at(id).assemble();
        assembler_map.erase(id);
        return message;
    }
};

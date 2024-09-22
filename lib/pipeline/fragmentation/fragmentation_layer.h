#pragma once

#include <map>
#include <mutex>

#include "pipeline/fragmentation/fragment_assembler.h"
#include "pipeline/fragmentation/fragmenter.h"
#include "pipeline/pipeline_step.h"

class FragmentationLayer final : public PipelineStep
{
    std::map<std::string, FragmentAssembler> assembler_map;

    Observer<ForwardDefragmentedMessage> obs_forward_defragmented_message;
    void forward_defragmented_message(const ForwardDefragmentedMessage& event);

    std::string get_message_identifier(Packet p)
    {
        return format("%s/%s", gr->get_node(p.meta.origin).get_address().to_string().c_str(), std::to_string(p.data.header.get_message_number()).c_str());
    }

public:
    FragmentationLayer(PipelineHandler handler, GroupRegistry *gr);
    ~FragmentationLayer() override;

    void attach(EventBus&);

    void send(Message) override;
    void send(Packet) override;

    void receive(Packet) override;
};

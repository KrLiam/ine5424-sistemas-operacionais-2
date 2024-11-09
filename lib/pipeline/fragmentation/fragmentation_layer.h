#pragma once

#include <unordered_map>
#include <mutex>

#include "pipeline/fragmentation/fragment_assembler.h"
#include "pipeline/fragmentation/fragmenter.h"
#include "pipeline/pipeline_step.h"

class FragmentationLayer final : public PipelineStep
{
    std::unordered_map<MessageIdentity, FragmentAssembler> assembler_map;

    Observer<ForwardDefragmentedMessage> obs_forward_defragmented_message;
    void forward_defragmented_message(const ForwardDefragmentedMessage& event);

public:
    FragmentationLayer(PipelineHandler handler);
    ~FragmentationLayer() override;

    void attach(EventBus&);

    void send(Message) override;
    void send(Packet) override;

    void receive(Packet) override;
};

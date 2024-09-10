#pragma once

#include <map>

#include "communication/fragment_assembler.h"
#include "communication/pipeline_step.h"
#include "communication/reliable_communication.h"

class ReliableCommunication;

class FragmentationLayer : public PipelineStep
{
private:
    ReliableCommunication *comm;

    std::map<std::string, FragmentAssembler> assembler_map;

public:
    FragmentationLayer(PipelineHandler& handler, ReliableCommunication *comm);

    void service();

    void send(char*m);

    void receive(char*m);
};

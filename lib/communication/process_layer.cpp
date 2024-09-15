#include "communication/process_layer.h"

ProcessLayer::ProcessLayer(PipelineHandler handler, GroupRegistry *gr) : PipelineStep(handler, gr)
{
}

ProcessLayer::~ProcessLayer()
{
}

void ProcessLayer::service()
{
}

void ProcessLayer::send(Message message)
{
    log_debug("Message [", message.to_string(), "] sent to process layer.");
    handler.forward_send(message);
}

void ProcessLayer::receive(Message message)
{
    log_debug("Message [", message.to_string(), "] received on process layer.");

    switch (message.type)
    {
    case MessageType::DATA:
        handler.forward_to_application(message); // TODO: acho q a reciver thread vai travar aqui caso o buffer esteja cheio. tratar
        break;

    default:
        break;
    }
}
#include "communication/process_layer.h"

ProcessLayer::ProcessLayer(PipelineHandler& handler, ReliableCommunication *comm) : PipelineStep(PipelineStep::PROCESS_LAYER, handler), comm(comm) {}

void ProcessLayer::service() {}

void ProcessLayer::send(char *m)
{
    Packet packet;
    memcpy(&packet, m, sizeof(Packet));
    log_debug("Packet [", packet.to_string(), "] sent to process layer.");
    forward_send(m);
}

void ProcessLayer::receive(char *m)
{
    Message message;
    memcpy(&message, m, sizeof(Message));
    log_debug("Message [", message.to_string(), "] received on process layer.");

    switch (message.type)
    {
    case MessageType::DATA:
        comm->produce(message); // dá pra usar friend pra esconder do usuário
        // TODO: acho q a reciver thread vai travar aqui caso o buffer esteja cheio. tratar
        break;

    default:
        break;
    }
}
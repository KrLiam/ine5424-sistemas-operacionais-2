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
    Node destination = gr->get_node(message.destination);
    Connection& connection = gr->get_connection(destination.get_id());

    // TODO: isso deve ser feito no proprio Connection ao iniciar a transmissão
    message.number = connection.get_next_message_number();
    connection.increment_next_message_number();

    log_debug("Message [", message.to_string(), "] sent to process layer.");
    handler.forward_send(message);
}

void ProcessLayer::receive(Message message)
{
    log_debug("Message [", message.to_string(), "] received on process layer.");

    // TODO: isso deve ser feito no proprio Connection ao encerrar o recebimento
    const Node& origin = gr->get_node(message.origin);
    Connection& connection = gr->get_connection(origin.get_id());
    connection.increment_expected_message_number();

    switch (message.type)
    {
    case MessageType::DATA:
        handler.forward_to_application(message); // TODO: acho q a reciver thread vai travar aqui caso o buffer esteja cheio. tratar
        break;

    default:
        break;
    }
}
#include "communication/reliable_communication.h"

ReliableCommunication::ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size)
    : user_buffer_size(_user_buffer_size)
{
    gr = new GroupRegistry(_local_id);
    const Node &local_node = gr->get_local_node();
    channel = new Channel(local_node.get_address());

    pipeline = new Pipeline(gr, channel);
}

ReliableCommunication::~ReliableCommunication()
{
    delete gr;
    delete pipeline;
    delete channel;
}

GroupRegistry *ReliableCommunication::get_group_registry()
{
    return gr;
}

void ReliableCommunication::send(std::string id, char *m)
{
    Message message = {
        origin : gr->get_local_node().get_address(),
        destination : gr->get_node(id).get_address(),
        type : MessageType::DATA, // TODO: Definir corretamente
        data : {0},
        length : user_buffer_size
    };
    strncpy(message.data, m, user_buffer_size);
    pipeline->send(&message.as_bytes()[0]);
}

Message ReliableCommunication::receive(char *m)
{
    Message message = pipeline->get_incoming_buffer().consume();

    if (user_buffer_size >= message.length)
    {
        strncpy(m, message.data, message.length);
        return message;
    }

    throw std::runtime_error("Provided buffer is smaller than the message.");
    /*log_warn("Buffer size defined by the user [", user_buffer_size , "] is smaller ",
    "than the received message's size [", message.length, "]; we will split this message into multiple ones.");
    int required_parts = ceil((double) message.length / user_buffer_size);
    for (int i = 0; i < required_parts; i++)
    {
        Message part = {
            origin: message.origin,
            destination: message.destination,
            type: message.type,
            part: i,
            has_more_parts: i != required_parts - 1
        };
        part.length = i * user_buffer_size;
        strncpy(part.data, &message.data[i * user_buffer_size], part.length);
        receive_buffer.produce(part);
    }

    return receive(m); // problema: perde a ordem */
}

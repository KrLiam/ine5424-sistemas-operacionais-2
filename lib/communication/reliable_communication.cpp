#include "communication/reliable_communication.h"

ReliableCommunication::ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size)
    : connection_update_buffer("connection_update"),
      user_buffer_size(_user_buffer_size)
{
    gr = new GroupRegistry(_local_id);
    const Node &local_node = gr->get_local_node();
    channel = new Channel(local_node.get_address());
    pipeline = new Pipeline(gr, channel);

    sender_thread = std::thread([this]() { send_routine(); });

    gr->establish_connections(*pipeline, application_buffer, connection_update_buffer);
}

ReliableCommunication::~ReliableCommunication()
{
    alive = false;

    if (sender_thread.joinable()) sender_thread.join();

    delete gr;
    delete pipeline;
    delete channel;
}

GroupRegistry *ReliableCommunication::get_group_registry()
{
    return gr;
}

Message ReliableCommunication::receive(char *m)
{
    Message message = application_buffer.consume();

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

bool ReliableCommunication::send(std::string id, MessageData data)
{
    if (data.size == std::size_t(-1))
        data.size = user_buffer_size;

    Transmission transmission = create_transmission(id, data);
    enqueue(transmission);
    
    TransmissionResult result = transmission.wait_result();
    log_debug("Transmission ", transmission.uuid, " returned result to application.");

    return result.success;
}

Message ReliableCommunication::create_message(std::string id, const MessageData& data) {
    Message m = {
        transmission_uuid : UUID(0, 0),
        number : 0,
        origin : gr->get_local_node().get_address(),
        destination : gr->get_node(id).get_address(),
        type : MessageType::APPLICATION, // TODO: Definir corretamente
        data : {0},
        length : data.size,
    };
    strncpy(m.data, data.ptr, data.size);
    return m;
}

Transmission ReliableCommunication::create_transmission(std::string sender_id, const MessageData& data) {
    Message message = create_message(sender_id, data);

    return Transmission(sender_id, message);
}

void ReliableCommunication::enqueue(Transmission& transmission) {   
    Connection& connection = gr->get_connection(transmission.receiver_id);
    connection.enqueue(transmission);
}

void ReliableCommunication::send_routine() {
    while (alive) {
        std::string id = connection_update_buffer.consume();
        Connection& connection = gr->get_connection(id);

        connection.update();
    }
}
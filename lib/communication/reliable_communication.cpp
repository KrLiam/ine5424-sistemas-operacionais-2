#include "communication/reliable_communication.h"

#include "pipeline/fault_injection/fault_injection_layer.h"

ReliableCommunication::ReliableCommunication(
    std::string _local_id,
    std::size_t _user_buffer_size) : ReliableCommunication(_local_id, _user_buffer_size, FaultConfig()) {}

ReliableCommunication::ReliableCommunication(
    std::string _local_id,
    std::size_t _user_buffer_size,
    FaultConfig fault_config)
    : connection_update_buffer("connection_update"),
      application_buffer(INTERMEDIARY_BUFFER_ITEMS),
      user_buffer_size(_user_buffer_size)
{
    gr = new GroupRegistry(_local_id);
    pipeline = new Pipeline(gr, fault_config);

    sender_thread = std::thread([this]()
                                { send_routine(); });

    gr->establish_connections(*pipeline, application_buffer, connection_update_buffer);
}

ReliableCommunication::~ReliableCommunication()
{
    connection_update_buffer.terminate();
    if (sender_thread.joinable())
        sender_thread.join();

    delete gr;
    delete pipeline;
}

void ReliableCommunication::shutdown() {
    application_buffer.terminate();
}

GroupRegistry *ReliableCommunication::get_group_registry()
{
    return gr;
}

ReceiveResult ReliableCommunication::receive(char *m)
{
    Message message = application_buffer.consume();

    std::size_t len = std::min(message.length, user_buffer_size);
    memcpy(m, message.data, len);

    if (len < message.length)
    {
        log_warn("User's buffer is smaller than the message; truncating it.");
    }

    Node node = gr->get_node(message.origin);
    return ReceiveResult{
        bytes : len,
        truncated_bytes : message.length - len,
        sender_address : message.origin,
        sender_id : node.get_id()
    };
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
        memcpy(part.data, &message.data[i * user_buffer_size], part.length);
        receive_buffer.produce(part);
    }

    return receive(m); // problema: perde a ordem */
}

bool ReliableCommunication::send(std::string id, MessageData data)
{
    if (data.size == std::size_t(-1))
        data.size = user_buffer_size;

    if (data.size > Message::MAX_SIZE)
    {
        log_error("Unable to send a message of ", data.size, " bytes. ",
                  "Maximum supported length is ", Message::MAX_SIZE, " bytes.");
        return false;
    }

    Transmission transmission = create_transmission(id, data);
    enqueue(transmission);

    TransmissionResult result = transmission.wait_result();
    log_debug("Transmission ", transmission.uuid, " returned result to application.");

    return result.success;
}

Message ReliableCommunication::create_message(std::string receiver_id, const MessageData &data)
{
    Message m = {
        transmission_uuid : UUID(""),
        number : 0,
        origin : gr->get_local_node().get_address(),
        destination : gr->get_node(receiver_id).get_address(),
        type : MessageType::APPLICATION, // TODO: Definir corretamente
        data : {0},
        length : data.size,
    };
    memcpy(m.data, data.ptr, data.size);
    return m;
}

Transmission ReliableCommunication::create_transmission(std::string receiver_id, const MessageData &data)
{
    Message message = create_message(receiver_id, data);

    return Transmission(receiver_id, message);
}

void ReliableCommunication::enqueue(Transmission &transmission)
{
    Connection &connection = gr->get_connection(transmission.receiver_id);
    connection.enqueue(transmission);
}

void ReliableCommunication::send_routine()
{
    std::string id;
    while (true)
    {
        try
        {
            id = connection_update_buffer.consume();
        }
        catch (const std::runtime_error &e)
        {
            return;
        }

        Connection &connection = gr->get_connection(id);
        connection.update();
    }
}
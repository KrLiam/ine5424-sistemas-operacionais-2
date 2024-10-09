#include "communication/reliable_communication.h"

#include "pipeline/fault_injection/fault_injection_layer.h"

ReliableCommunication::ReliableCommunication(
    std::string local_id,
    std::size_t user_buffer_size
) : ReliableCommunication(local_id, user_buffer_size, FaultConfig()) {}

ReliableCommunication::ReliableCommunication(
    std::string local_id,
    std::size_t user_buffer_size,
    FaultConfig fault_config
) :
    connection_update_buffer("connection_update"),
    user_buffer_size(user_buffer_size),
    application_buffer(INTERMEDIARY_BUFFER_ITEMS)
{
    Config config = ConfigReader::parse_file("nodes.conf");

    gr = std::make_shared<GroupRegistry>(local_id, config);
    pipeline = new Pipeline(gr, event_bus, fault_config);

    sender_thread = std::thread([this]()
                                { send_routine(); });

    gr->establish_connections(
        *pipeline,
        application_buffer,
        application_buffer, // TODO substutir por deliver_buffer posteriormente, reusando o msm buffer pq dai vai precisar mudar o programa de testes
        connection_update_buffer
    );
    failure_detection = std::make_unique<FailureDetection>(gr, event_bus, config.alive);
}

ReliableCommunication::~ReliableCommunication()
{
    connection_update_buffer.terminate();
    if (sender_thread.joinable())
        sender_thread.join();

    delete pipeline;
}

void ReliableCommunication::shutdown() {
    application_buffer.terminate();
}

std::shared_ptr<GroupRegistry> ReliableCommunication::get_group_registry()
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

    Node node = gr->get_nodes().get_node(message.id.origin);
    return ReceiveResult{
        length : len,
        truncated_bytes : message.length - len,
        sender_address : message.id.origin,
        sender_id : node.get_id()
    };
}

bool ReliableCommunication::send(std::string id, MessageData data)
{
    if (data.size == std::size_t(-1))
        data.size = user_buffer_size;

    if (data.size > Message::MAX_SIZE)
    {
        log_error(
            "Unable to send a message of ", data.size, " bytes. ",
            "Maximum supported length is ", Message::MAX_SIZE, " bytes."
        );
        return false;
    }

    Transmission transmission = create_transmission(id, data, MessageType::SEND);
    bool enqueued = gr->enqueue(transmission);

    if (!enqueued) {
        log_warn(
            "Could not enqueue transmission ", transmission.uuid,
            ", node connection must be overloaded."
        );
        return false;
    }

    TransmissionResult result = transmission.wait_result();
    log_debug("Transmission ", transmission.uuid, " returned result to application.");

    return result.success;
}

bool ReliableCommunication::broadcast(MessageData data) {
    if (data.size == std::size_t(-1))
        data.size = user_buffer_size;

    if (data.size > Message::MAX_SIZE)
    {
        log_error(
            "Unable to broadcast message of ", data.size, " bytes. ",
            "Maximum supported length is ", Message::MAX_SIZE, " bytes."
        );
        return false;
    }

    Transmission transmission = create_transmission(BROADCAST_ID, data, MessageType::URB);
    gr->enqueue(transmission);
    
    TransmissionResult result = transmission.wait_result();
    log_debug("Transmission ", transmission.uuid, " returned result to application.");

    return result.success;
}

Message ReliableCommunication::create_message(std::string receiver_id, const MessageData &data, MessageType msg_type) {
    const Node& receiver = gr->get_nodes().get_node(receiver_id);
    return create_message(receiver.get_address(), data, msg_type);
}
Message ReliableCommunication::create_message(SocketAddress receiver_address, const MessageData &data, MessageType msg_type)
{
    const Node& local = gr->get_local_node();

    Message m = {
        id : {
            origin : local.get_address(),
            msg_num : 0,
            sequence_type : receiver_address == BROADCAST_ADDRESS ?
                MessageSequenceType::BROADCAST : MessageSequenceType::UNICAST
        },
        transmission_uuid : UUID(""),
        origin : local.get_address(),
        destination : receiver_address,
        type : msg_type,
        data : {0},
        length : data.size,
    };
    memcpy(m.data, data.ptr, data.size);

    return m;
}

Transmission ReliableCommunication::create_transmission(
    std::string receiver_id, const MessageData &data, MessageType msg_type
)
{
    Message message;
    
    if (receiver_id == BROADCAST_ID) {
        message = create_message({BROADCAST_ADDRESS, 0}, data, msg_type);
    }
    else {
        message = create_message(receiver_id, data, msg_type);
    }

    return Transmission(message, receiver_id);
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

        gr->update(id);
    }
}
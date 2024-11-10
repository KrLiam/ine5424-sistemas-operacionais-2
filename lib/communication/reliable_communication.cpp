#include "communication/reliable_communication.h"

#include "pipeline/fault_injection/fault_injection_layer.h"

ReliableCommunication::ReliableCommunication(
    std::string local_id,
    std::size_t user_buffer_size,
    const Config& config
) :
    config(config),
    connection_update_buffer("connection_update"),
    user_buffer_size(user_buffer_size),
    application_buffer(INTERMEDIARY_BUFFER_ITEMS),
    deliver_buffer(INTERMEDIARY_BUFFER_ITEMS)
{
    const NodeConfig& node_config = config.get_node(local_id);
    log_info("Initializing node ", node_config.id, " (", node_config.address.to_string(), ")."); 

    gr = std::make_shared<GroupRegistry>(local_id, config, event_bus);
    pipeline = std::make_unique<Pipeline>(gr, event_bus, config.faults);

    sender_thread = std::thread([this]()
                                { send_routine(); });

    gr->establish_connections(
        *pipeline,
        application_buffer,
        deliver_buffer,
        connection_update_buffer,
        config.alive
    );
    failure_detection = std::make_unique<FailureDetection>(gr, event_bus, config.alive);
}

ReliableCommunication::~ReliableCommunication()
{
    Node& node = gr->get_local_node();
    log_info("Killing node ", node.get_id(), " (", node.get_address().to_string(), ")."); 

    application_buffer.terminate();
    deliver_buffer.terminate();
    
    failure_detection->terminate();

    connection_update_buffer.terminate();
    if (sender_thread.joinable())
        sender_thread.join();
}

void ReliableCommunication::add_fault_rule(const FaultRule& rule) {
    FaultInjectionLayer& fault_layer = pipeline->get_fault_layer();
    fault_layer.add_rule(rule);
}

std::shared_ptr<GroupRegistry> ReliableCommunication::get_group_registry()
{
    return gr;
}

ReceiveResult ReliableCommunication::message_to_buffer(Message &message, char *m) {
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

ReceiveResult ReliableCommunication::receive(char *m)
{
    Message message = application_buffer.consume();
    return message_to_buffer(message, m);
}

ReceiveResult ReliableCommunication::deliver(char *m)
{
    Message message = deliver_buffer.consume();
    return message_to_buffer(message, m);
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

    MessageType message_type = message_type::from_broadcast_type(config.broadcast);
    std::string receiver_id = message_type == MessageType::AB_REQUEST ? LEADER_ID : BROADCAST_ID;
    Transmission transmission = create_transmission(receiver_id, data, message_type);
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
            msg_type : msg_type,
        },
        transmission_uuid : UUID(""),
        origin : local.get_address(),
        destination : receiver_address,
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
    
    if (receiver_id == BROADCAST_ID || receiver_id == LEADER_ID) {
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
        catch (const buffer_termination &e)
        {
            return;
        }

        gr->update(id);
    }
}
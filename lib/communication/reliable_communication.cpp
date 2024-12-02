#include "communication/reliable_communication.h"

#include "pipeline/fault_injection/fault_injection_layer.h"

ReliableCommunication::ReliableCommunication(
    std::string local_id,
    std::size_t user_buffer_size,
    bool verbose,
    const Config& cfg
) :
    config(cfg),
    user_buffer_size(user_buffer_size)
{
    const NodeConfig* node_config = config.get_node(local_id);
    if (!node_config) throw std::invalid_argument(format("Node '%s' does not exist in the config.", local_id.c_str()));

    log_info("Initializing node ", node_config->id, " (", node_config->address.to_string(), ").");

    gr = std::make_unique<GroupRegistry>(local_id, config, event_bus);
    pipeline = std::make_unique<Pipeline>(gr.get(), event_bus, config.faults);

    sender_thread = std::thread([this]()
                                { send_routine(); });

    gr->set_pipeline(pipeline.get());
    gr->establish_connections();

    failure_detection = std::make_unique<FailureDetection>(
        gr, event_bus, config.alive, verbose
    );

    gr->start_raft();
}

ReliableCommunication::~ReliableCommunication()
{
    Node& node = gr->get_local_node();
    log_info("Killing node ", node.get_id(), " (", node.get_address().to_string(), ")."); 

    gr->get_application_buffer().terminate();
    gr->get_deliver_buffer().terminate();
    
    failure_detection->terminate();

    gr->get_connection_update_buffer().terminate();
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
    Message message = gr->get_application_buffer().consume();
    return message_to_buffer(message, m);
}

ReceiveResult ReliableCommunication::deliver(char *m)
{
    Message message = gr->get_deliver_buffer().consume();
    return message_to_buffer(message, m);
}

bool ReliableCommunication::send(std::string group_id, std::string id, MessageData data)
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

    auto joined_groups = get_joined_groups();
    uint64_t group_hash;

    if (joined_groups.contains(group_id)) {
        const GroupInfo& group = joined_groups.at(group_id);
        group_hash = group.hash;
    }
    else if (group_id == GLOBAL_GROUP_ID) {
        group_hash = 0;
    }
    else {
        log_error("Unable to send a message on group '", group_id, "' before joining it.");
        return false;
    }

    Transmission transmission = create_transmission(group_hash, id, data, MessageType::SEND);
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

bool ReliableCommunication::broadcast(std::string group_id, MessageData data) {
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

    auto joined_groups = get_joined_groups();
    uint64_t group_hash = 0;

    if (joined_groups.contains(group_id)) {
        const GroupInfo& group = joined_groups.at(group_id);
        group_hash = group.hash;
    }
    else if (group_id == GLOBAL_GROUP_ID) {
        group_hash = 0;
    }
    else {
        log_error("Unable to send a message on group '", group_id, "' before joining it.");
        return false;
    }

    MessageType message_type = message_type::from_broadcast_type(config.broadcast);
    std::string receiver_id = message_type == MessageType::AB_REQUEST ? LEADER_ID : BROADCAST_ID;
    Transmission transmission = create_transmission(group_hash, receiver_id, data, message_type);
    gr->enqueue(transmission);
    
    TransmissionResult result = transmission.wait_result();
    log_debug("Transmission ", transmission.uuid, " returned result to application.");

    return result.success;
}

Message ReliableCommunication::create_message(uint64_t group_hash, std::string receiver_id, const MessageData &data, MessageType msg_type) {
    const Node& receiver = gr->get_nodes().get_node(receiver_id);
    return create_message(group_hash, receiver.get_address(), data, msg_type);
}
Message ReliableCommunication::create_message(uint64_t group_hash, SocketAddress receiver_address, const MessageData &data, MessageType msg_type)
{
    const Node& local = gr->get_local_node();

    Message m = {
        group_hash : group_hash,
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
    uint64_t group_hash, std::string receiver_id, const MessageData &data, MessageType msg_type
)
{
    Message message;
    
    if (receiver_id == BROADCAST_ID || receiver_id == LEADER_ID) {
        message = create_message(group_hash, {BROADCAST_ADDRESS, 0}, data, msg_type);
    }
    else {
        message = create_message(group_hash, receiver_id, data, msg_type);
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
            id = gr->get_connection_update_buffer().consume();
        }
        catch (const buffer_termination &e)
        {
            return;
        }

        gr->update(id);
    }
}


bool ReliableCommunication::register_group(std::string id, ByteArray key) {
    if (config.groups.contains(id)) return false;
    config.groups.emplace(id, key);

    return true;
}

bool ReliableCommunication::join_group(std::string id)
{
    if (!config.groups.contains(id)) throw std::invalid_argument(
        format("Group '%s' is not registered.", id.c_str())
    );

    const ByteArray& key = config.groups.at(id);
    event_bus.notify(JoinGroup(id, key));

    return true;
}

bool ReliableCommunication::leave_group(std::string id)
{
    if (!config.groups.contains(id)) throw std::invalid_argument(
        format("Group '%s' is not registered.", id.c_str())
    );

    const ByteArray& key = config.groups.at(id);
    uint64_t key_hash = std::hash<ByteArray>()(key);

    event_bus.notify(LeaveGroup(id, key_hash));

    return true;
}

void ReliableCommunication::discover_node(const SocketAddress& address) {
    failure_detection->discover(address, true);
}

std::unordered_map<std::string, GroupInfo> ReliableCommunication::get_joined_groups() {
    auto& group_map = pipeline->get_encryption_layer().get_groups();

    std::unordered_map<std::string, GroupInfo> joined;
    for (const auto& [_, group] : group_map) {
        joined.emplace(group.id, group);
    }
    return joined;
}
std::unordered_map<std::string, GroupInfo> ReliableCommunication::get_available_groups() {
    auto& group_map = pipeline->get_encryption_layer().get_groups();

    std::unordered_map<std::string, GroupInfo> available;
    for (const auto& [id, key] : config.groups) {
        GroupInfo group(id, key);
        if (group_map.contains(group.hash)) continue;

        available.emplace(group.id, group);
    }

    return available;
}

std::unordered_map<uint64_t, GroupInfo> ReliableCommunication::get_registered_groups() {
    std::unordered_map<uint64_t, GroupInfo> groups;

    for (const auto& [id, key] : config.groups) {
        GroupInfo group(id, key);
        groups.emplace(group.hash, group);
    }

    return groups;
}
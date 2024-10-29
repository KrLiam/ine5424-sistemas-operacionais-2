#include "communication/group_registry.h"
#include "pipeline/pipeline.h"

GroupRegistry::GroupRegistry(std::string local_id, Config config, EventBus& event_bus)
    : event_bus(event_bus), local_id(local_id)
{
    attach_observers();
    read_nodes_from_configuration(local_id, config);
}

GroupRegistry::~GroupRegistry()
{
}


void GroupRegistry::attach_observers() {
    obs_leader_elected.on(std::bind(&GroupRegistry::leader_elected, this, _1));
    event_bus.attach(obs_leader_elected);
}

NodeMap& GroupRegistry::get_nodes()
{
    return nodes;
}

Node &GroupRegistry::get_local_node()
{
    return nodes.get_node(local_id);
}

void GroupRegistry::read_nodes_from_configuration(std::string local_id, Config config)
{
    nodes.clear();
    for (NodeConfig node_config : config.nodes)
    {
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        nodes.add(node);
    }
}

void GroupRegistry::leader_elected(const LeaderElected&) {
    update_leader_queue();
}

void GroupRegistry::update_leader_queue() {
    Node* leader = nodes.get_leader();

    if (!leader) {
        log_warn("No leader is elected right now. Transmission will hang.");
        return;
    }

    while (!leader_transmissions.empty()) {
        Transmission* next = leader_transmissions.front();
        leader_transmissions.pop();

        Message& message = next->message;
        message.destination = leader->get_address();
        next->receiver_id = leader->get_id();

        Connection& conn = connections.at(leader->get_id());
        bool enqueued = conn.enqueue(*next);
        if (!enqueued) {
            next->set_result(false);
            next->release();
        }
    }
}

bool GroupRegistry::enqueue(Transmission& transmission) {
    if (transmission.to_leader()) {
        leader_transmissions.push(&transmission);
        update_leader_queue();
        return true;
    }

    if (transmission.is_broadcast()) {
        return broadcast_connection->enqueue(transmission);
    }

    Connection& connection = get_connection(transmission.receiver_id);
    return connection.enqueue(transmission);
}

void GroupRegistry::update(std::string id) {
    if (id == BROADCAST_ID) {
        broadcast_connection->update();
        return;
    }

    Connection &connection = get_connection(id);
    connection.update();
}


void GroupRegistry::establish_connections(
    Pipeline &pipeline,
    Buffer<Message> &application_buffer,
    Buffer<Message> &deliver_buffer,
    BufferSet<std::string> &connection_update_buffer,
    unsigned int alive
) {
    Node& local_node = get_local_node();

    broadcast_connection =  std::make_unique<BroadcastConnection>(
        nodes, local_node, connections, connection_update_buffer, deliver_buffer, pipeline
    );

    for (auto &[id, node] : nodes)
        connections.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(
                local_node,
                node,
                pipeline,
                application_buffer,
                connection_update_buffer,
                broadcast_connection->get_dispatcher(),
                broadcast_connection->get_ab_dispatcher()
            )
        );
    
    raft = std::make_unique<RaftManager>(
        *broadcast_connection, connections, nodes, local_node, event_bus, alive
    );
}
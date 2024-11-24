#include "failure_detection.h"
#include "utils/date.h"

FailureDetection::FailureDetection(
    std::shared_ptr<GroupRegistry> gr,
    EventBus &event_bus,
    unsigned int alive,
    bool verbose
)
    : gr(gr), event_bus(event_bus), alive(alive), running(true), verbose(verbose)
{
    attach();

    timer_id = TIMER.add(alive, [this]() { failure_detection_routine(); });
    for (auto& [id, node] : gr->get_nodes()) heartbeat(node);
}

FailureDetection::~FailureDetection()
{
    terminate();
}

void FailureDetection::terminate() {
    running = false;
    if (timer_id >= 0) TIMER.cancel(timer_id);
}

void FailureDetection::connection_established(const ConnectionEstablished &event)
{
    last_alive.emplace(event.node.get_id(), DateUtils::now());
}

void FailureDetection::connection_closed(const ConnectionClosed &event)
{
    last_alive.erase(event.node.get_id());
}

void FailureDetection::packet_received(const PacketReceived &event)
{
    Packet& packet = event.packet;
    Node& node = gr->get_nodes().get_node(packet.meta.origin);

    log_trace("Received ", packet.to_string(), " (node ", node.get_id(), ").");

    const UUID uuid = UUID(std::string(event.packet.data.header.uuid));

    mtx.lock();
    if (node.is_alive() && uuid != node.get_uuid())
    {
        log_info(
            "Node ",
            node.get_address().to_string(),
            " restarted. Notifying about its death before marking it as active.");
        node.set_state(FAULTY);
        event_bus.notify(NodeDeath(node));
    }

    last_alive[node.get_id()] = DateUtils::now();

    NodeState state = node.get_state();
    if (state != ACTIVE)
    {
        node.set_uuid(uuid);
        node.set_state(ACTIVE);

        std::unordered_set<SocketAddress> &suspicions = get_suspicions(gr->get_local_node().get_id());
        suspicions.erase(node.get_address());
        // Talvez dava pra remover esse nó da lista de suspeitos dos outros nós tb

        log_info(
            "Received ",
            event.packet.to_string(PacketFormat::RECEIVED),
            " (node ",
            node.get_id(),
            "). Marking it as active."
        );
        if (state == NOT_INITIALIZED || state == FAULTY) event_bus.notify(NodeUp(node));
    }

    if (packet.data.header.get_message_type() == MessageType::HEARTBEAT) process_heartbeat_data(packet);
    mtx.unlock();

}

void FailureDetection::packet_sent(const PacketSent &event) {
    Node& node = gr->get_nodes().get_node(event.packet.meta.destination);
    schedule_heartbeat(node);
}

void FailureDetection::process_heartbeat_data(const Packet& packet)
{
    Node& remote = gr->get_nodes().get_node(packet.meta.origin);

    if (verbose) {
        log_info("Received heartbeat from ", remote.get_id(), " (", packet.meta.origin.to_string(), ")");
    }

    int num_of_suspicions = packet.meta.message_length / SocketAddress::SERIALIZED_SIZE;
    if (num_of_suspicions == 0) return;


    std::unordered_set<SocketAddress> &remote_suspicions = get_suspicions(remote.get_id());
    remote_suspicions.clear();

    const HeartbeatData* data = reinterpret_cast<const HeartbeatData*>(packet.data.message_data);

    for (int i = 0; i < num_of_suspicions; i++) {
        if ((i + 1) * SocketAddress::SERIALIZED_SIZE > PacketData::MAX_MESSAGE_SIZE) break;

        SocketAddress suspicion = SocketAddress::deserialize(&data->suspicions[i * SocketAddress::SERIALIZED_SIZE]);
        log_debug("Node ", remote.get_id(), " suspects of ", suspicion.to_string());
        remote_suspicions.insert(suspicion);
    }

    check_for_faulty_nodes();
}

void FailureDetection::check_for_faulty_nodes()
{
    NodeMap& nodes = gr->get_nodes();
    unsigned int quorum = (nodes.size() / 2) + 1;

    std::unordered_map<SocketAddress, unsigned int> suspicion_count = count_suspicions();

    for (auto &[address, count] : suspicion_count) {
        if (count < quorum) continue;

        Node &node = nodes.get_node(address);
        if (node.get_state() == FAULTY) continue;

        log_warn(
                "Majority of nodes (", quorum, ") suspect of node '",
                node.get_id(),
                "' (",
                node.get_address().to_string(),
                "); marking it as faulty."
            );
        node.set_state(FAULTY);

        std::unordered_set<SocketAddress> &local_suspicions = get_suspicions(gr->get_local_node().get_id());
        local_suspicions.erase(node.get_address());

        event_bus.notify(NodeDeath(node));
    }
}

std::unordered_map<SocketAddress, unsigned int> FailureDetection::count_suspicions()
{
    std::unordered_map<SocketAddress, unsigned int> suspicion_count;

    for (auto &[id, suspicions] : suspicion_map) {
        for (const SocketAddress &address : suspicions) {
            if (!suspicion_count.contains(address)) suspicion_count.emplace(address, 0);
            suspicion_count[address]++;
        }
    }

    return suspicion_count;
}

std::unordered_set<SocketAddress> &FailureDetection::get_suspicions(std::string node_id)
{
    if (!suspicion_map.contains(node_id)) suspicion_map.emplace(node_id, std::unordered_set<SocketAddress>{});
    return suspicion_map[node_id];
}

void FailureDetection::failure_detection_routine()
{
    if (!running) return;

    mtx.lock();

    Node& local_node = gr->get_local_node();
    std::unordered_set<SocketAddress> &suspicions = get_suspicions(local_node.get_id());
    suspicions.clear();

    for (auto &[id, node] : gr->get_nodes())
    {
        if (node.get_address() == local_node.get_address()) continue;

        NodeState state = node.get_state();
        if (state == ACTIVE && DateUtils::now() - last_alive[id] > alive * ALIVE_TOLERANCE)
        {
            node.set_state(SUSPECT);
            
            log_warn(
                "No heartbeat from node '",
                id,
                "' (",
                node.get_address().to_string(),
                ") in the last ",
                alive,
                " milliseconds; marking it as suspect. "
            );
            std::unordered_set<SocketAddress> &suspicions = get_suspicions(gr->get_local_node().get_id());
            suspicions.insert(node.get_address());
        }
        else if (state == SUSPECT) suspicions.insert(node.get_address());
    }

    mtx.unlock();

    if (!suspicions.empty()) {
        for (auto &[_, node] : gr->get_nodes()) heartbeat(node);
    }


    timer_id = TIMER.add(alive, [this]() { failure_detection_routine(); });
}

void FailureDetection::heartbeat(const Node& node)
{
    if (!running) return;

    mtx.lock();
    std::unordered_set<SocketAddress> &suspicions = get_suspicions(gr->get_local_node().get_id());
    Connection &conn = gr->get_connection(node);
    conn.heartbeat(suspicions);
    mtx.unlock();

    schedule_heartbeat(node);
}

void FailureDetection::schedule_heartbeat(const Node& node)
{
    std::string id = node.get_id();

    hb_timers_mtx.lock();
    if (hb_timers.contains(id)) TIMER.cancel(hb_timers[id]);
    hb_timers[id] = TIMER.add(alive, [this, node]() { heartbeat(node); });
    hb_timers_mtx.unlock();
}

void FailureDetection::attach()
{
    obs_connection_established.on(std::bind(&FailureDetection::connection_established, this, _1));
    obs_connection_closed.on(std::bind(&FailureDetection::connection_closed, this, _1));
    obs_packet_received.on(std::bind(&FailureDetection::packet_received, this, _1));
    obs_packet_sent.on(std::bind(&FailureDetection::packet_sent, this, _1));
    event_bus.attach(obs_connection_established);
    event_bus.attach(obs_connection_closed);
    event_bus.attach(obs_packet_received);
    event_bus.attach(obs_packet_sent);
}

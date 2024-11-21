#include "raft/raft.h"

#include <sys/stat.h>

RaftManager::RaftManager(
    BroadcastConnection &broadcast_connection,
    std::map<std::string, Connection> &connections,
    NodeMap &nodes,
    Node &local_node,
    EventBus &event_bus,
    unsigned int alive
) :
    data_filename(format(DATA_DIR "/raft/%s.raft", local_node.get_id().c_str())),
    state(FOLLOWER),
    broadcast_connection(broadcast_connection),
    connections(connections),
    nodes(nodes),
    local_node(local_node),
    leader(nullptr),
    event_bus(event_bus),
    timer_id(0),
    election_time_dis(alive * 6, alive * 7)
{
    obs_packet_received.on(std::bind(&RaftManager::packet_received, this, _1));
    event_bus.attach(obs_packet_received);

    set_election_timer();
}

void RaftManager::change_state(RaftState new_state) {
    if (state == new_state) return;

    state = new_state;

    cancel_election_timer();

    if (post_transition_handlers.contains(state))
        post_transition_handlers.at(state)();
}


void RaftManager::send_vote(Packet packet) {
    bool granted = should_grant_vote(packet.meta.origin);
    if (granted) {
        if (leader != nullptr && leader->get_address() != packet.meta.origin) {
            log_debug(
                "Received a vote request, but we already have a leader; "
                "ignoring.");
            return;
        }
        if (data.voted_for != nullptr && data.voted_for->get_address() != packet.meta.origin) {
            log_debug("Received vote request, but we already voted for someone.");
            return;
        }
        data.voted_for = &nodes.get_node(packet.meta.origin);
        set_election_timer();
    }

    log_info(granted ? "Voting for " : "Not voting for ", packet.meta.origin.to_string(), ".");

    RequestVoteData rvo_data = {
        granted: granted
    };

    PacketData data = packet.data;
    data.header.flags = data.header.flags | ACK;
    // data.header.id.origin = local_node.get_address();
    memcpy(data.message_data, &rvo_data, sizeof(RequestVoteData));

    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        origin : local_node.get_address(),
        destination : packet.meta.origin,
        message_length : sizeof(RequestVoteData),
        expects_ack : 0,
        silent : 0
    };

    Packet p = {data : data, meta : meta};

    Node &destination = nodes.get_node(p.meta.destination);
    Connection &connection = connections.at(destination.get_id());
    connection.dispatch_to_sender(p);
}

void RaftManager::send_request_vote() {
    PacketData data;
    memset(&data, 0, sizeof(PacketData));
    data.header = {
        id : {
            origin : local_node.get_address(),
            msg_num : 0,
            msg_type : MessageType::RAFT
        },
        fragment_num : 0,
        checksum : 0,
        flags : RVO | END,
        uuid: {0}
    };

    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        origin : local_node.get_address(),
        destination : {BROADCAST_ADDRESS, 0},
        message_length : 0,
        expects_ack : 1,
        silent : 0
    };

    Packet p = {data : data, meta : meta};

    broadcast_connection.dispatch_to_sender(p);;
}


void RaftManager::set_election_timer() {
    cancel_election_timer();
    int election_time = election_time_dis(rc_random::gen);
    timer_id = TIMER.add(election_time, election_timeout_handlers.at(state));
}

void RaftManager::cancel_election_timer() {
    if (timer_id != 0) TIMER.cancel(timer_id);
}


void RaftManager::follow(Node &node) {
    log_info("Following new leader, node ", node.get_id(), " (", node.get_address().to_string(), ").");

    MessageIdentity rvo_id = {
        origin : local_node.get_address(),
        msg_num : 0,
        msg_type : MessageType::RAFT
        };
    SocketAddress broadcast_address = {BROADCAST_ADDRESS, 0};
    event_bus.notify(PipelineCleanup(rvo_id, broadcast_address));

    if (leader != nullptr) leader->set_leader(false);
    leader = &node;
    leader->set_leader(true);

    cancel_election_timer();

    change_state(FOLLOWER);

    event_bus.notify(LeaderElected());
}

bool RaftManager::should_grant_vote(SocketAddress address) {
    for (auto &[id, node] : nodes) {
        if (!node.is_alive()) continue;
        if (strcmp(node.get_address().to_string().c_str(),
                   address.to_string().c_str()) < 0) {
            log_debug("Not granting vote to ", address.to_string(), "; ",
                      node.get_address().to_string(),
                      " has a higher priority.");
            return false;
        }
    }

    return true;
}

void RaftManager::check_if_won_election() {
    unsigned int quorum = get_quorum();
    if (received_votes.size() >= quorum) {
        log_info("Received ", received_votes.size(), " vote(s) (quorum is ", quorum, "), local node is now leader.");
        cancel_election_timer();
        change_state(LEADER);
        event_bus.notify(LeaderElected());
    }
}

unsigned int RaftManager::get_quorum() {
    unsigned int alive = 0;
    for (auto &[_, node] : nodes)
        if (node.is_alive()) alive++;

    return (alive / 2) + 1;
}


void RaftManager::packet_received(const PacketReceived &event) {
    Packet &packet = event.packet;
    MessageType type = packet.data.header.get_message_type();

    // if (type != MessageType::RAFT && type != MessageType::HEARTBEAT) return;

    if (!packet.silent()) {
        log_trace("Packet ", packet.to_string(PacketFormat::RECEIVED),
                  " received by raft manager.");
    }

    if (type == MessageType::RAFT && packet.data.header.is_ack()) event_bus.notify(PacketAckReceived(packet));

    packet_receive_handlers.at(state)(packet);
}


void RaftManager::on_follower() {
    data.voted_for = nullptr;

    set_election_timer();
}

void RaftManager::follower_timeout() { 
    if (leader != nullptr) leader->set_leader(false);
    change_state(CANDIDATE);
}

void RaftManager::follower_receive(Packet packet) {
    if (packet.data.header.is_ldr()) {
        set_election_timer();
        if (leader == nullptr || leader->get_address() != packet.meta.origin)
            follow(nodes.get_node(packet.meta.origin));
        return;
    }

    if (packet.data.header.is_rvo()) {
        if (packet.data.header.is_ack()) {
            log_debug("Received a vote confirmation, but we are not a candidate; ignoring.");
            return;
        }
        send_vote(packet);
    }
}


void RaftManager::on_candidate() { candidate_timeout(); }

void RaftManager::candidate_timeout() {
    log_info("Candidate timed out; starting new election.");
    data.voted_for = nullptr;
    leader = nullptr;

    received_votes.clear();

    set_election_timer();
    send_request_vote();
    check_if_won_election();
}

void RaftManager::candidate_receive(Packet packet) {
    if (packet.data.header.is_ldr()) {
        follow(nodes.get_node(packet.meta.origin));
        return;
    }

    if (packet.data.header.is_rvo()) {
        if (packet.data.header.is_ack()) {
            RequestVoteData* data = reinterpret_cast<RequestVoteData*>(packet.data.message_data);
            if (data->granted) {
                log_debug("Received vote from ", packet.meta.origin.to_string(), ".");
                received_votes.emplace(packet.meta.origin);
                check_if_won_election();
            }
            return;
        }
        send_vote(packet);
    }
}


void RaftManager::on_leader() {
    leader = &local_node;
    leader->set_leader(true);
}

void RaftManager::leader_receive(Packet packet) {
    if (packet.data.header.is_ldr()) {
        if (packet.meta.origin != local_node.get_address())
            follow(nodes.get_node(packet.meta.origin));
    }
}

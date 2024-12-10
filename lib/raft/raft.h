#pragma once

#include <functional>
#include <map>
#include <random>
#include <unordered_set>

#include "core/node.h"
#include "core/packet.h"
#include "communication/broadcast_connection.h"
#include "communication/transmission.h"
#include "utils/date.h"

using namespace std::placeholders;

enum RaftState
{
    FOLLOWER = 0,
    CANDIDATE = 1,
    LEADER = 2
};

class RaftManager
{
    Node *voted_for;
    RaftState state;

    BroadcastConnection& broadcast_connection;
    std::map<std::string, Connection> &connections;

    NodeMap &nodes;
    Node &local_node;
    Node *leader;

    EventBus &event_bus;
    Timer& timer;

    std::unordered_set<SocketAddress> received_votes;

    int timer_id;
    std::uniform_int_distribution<> election_time_dis;

    Observer<PacketReceived> obs_packet_received;

    const std::map<RaftState, std::function<void()>> post_transition_handlers = {
        {FOLLOWER, std::bind(&RaftManager::on_follower, this)},
        {CANDIDATE, std::bind(&RaftManager::on_candidate, this)},
        {LEADER, std::bind(&RaftManager::on_leader, this)}};

    const std::map<RaftState, std::function<void()>> election_timeout_handlers = {
        {FOLLOWER, std::bind(&RaftManager::follower_timeout, this)},
        {CANDIDATE, std::bind(&RaftManager::candidate_timeout, this)}};

    const std::map<RaftState, std::function<void(Packet)>> packet_receive_handlers = {
        {FOLLOWER, std::bind(&RaftManager::follower_receive, this, _1)},
        {CANDIDATE, std::bind(&RaftManager::candidate_receive, this, _1)},
        {LEADER, std::bind(&RaftManager::leader_receive, this, _1)}};

    void change_state(RaftState new_state);

    void set_election_timer();
    void cancel_election_timer();

    void follow(Node &node);
    bool should_grant_vote(SocketAddress address);
    void check_if_won_election();
    unsigned int get_quorum();

    void send_vote(Packet packet);
    void send_request_vote();

    void packet_received(const PacketReceived &event);

    void on_follower();
    void follower_timeout();
    void follower_receive(Packet packet);

    void on_candidate();
    void candidate_timeout();
    void candidate_receive(Packet packet);

    void on_leader();
    void leader_receive(Packet packet);
public:
    RaftManager(BroadcastConnection& broadcast_connection, std::map<std::string, Connection> &connections, NodeMap &nodes, Node &local_node, EventBus &event_bus, Timer& timer, unsigned int alive);
};

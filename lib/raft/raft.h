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

struct RaftPersistentData
{
    Node* voted_for;
};

class RaftManager
{
    std::string data_filename;

    RaftPersistentData data;

    RaftState state;

    std::map<std::string, Connection>& connections;
    // BroadcastConnection& broadcast_connection;

    NodeMap& nodes;
    Node& local_node;
    Node* leader;

    Pipeline& pipeline;

    std::unordered_set<SocketAddress> received_votes;

    int timer_id;
    Timer timer;
    std::uniform_int_distribution<> election_time_dis;

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

    void read_data()
    {
        std::ifstream f(data_filename);

        if (!f.good())
        {
            memset(&data, 0, sizeof(RaftPersistentData));
            return;
        }

        f.read((char*)&data, sizeof(RaftPersistentData));
        f.close();
    }

    void save_data()
    {
        std::ofstream f(data_filename);
        f.write((char*)&data, sizeof(RaftPersistentData));
        f.close();
    }

    void change_state(RaftState new_state)
    {
        if (state == new_state) return;

        state = new_state;

        cancel_election_timer();

        if (post_transition_handlers.contains(state)) post_transition_handlers.at(state)();
    }

    void follower_timeout()
    {
        change_state(CANDIDATE);
    }

    void follower_receive(Packet packet)
    {
        if (packet.data.header.is_ldr())
        {
            set_election_timer();
            if (leader == nullptr || leader->get_address() != packet.meta.origin) follow(nodes.get_node(packet.meta.origin));
            return;
        }
        
        if (packet.data.header.is_rvo())
        {
            if (packet.data.header.is_ack())
            {
                log_debug("Received a vote, but we are not a candidate; ignoring.");
                return;
            }

            if (leader != nullptr)
            {
                log_debug("Received a vote request, but we already have a leader; ignoring.");
                return;
            }

            if (data.voted_for != nullptr)
            {
                log_debug("Received vote request, but we already voted for someone.");
                return;
            }

            RaftRPCData* rvo_data = reinterpret_cast<RaftRPCData*>(packet.data.message_data);

            data.voted_for = &nodes.get_node(packet.meta.origin);
            save_data();

            if (should_grant_vote(packet.meta.origin))
            {
                send_vote(packet);
                set_election_timer();
            }
        }
    }

    void on_follower()
    {
        data.voted_for = nullptr;
        save_data();

        set_election_timer();
    }

    void send_vote(Packet packet)
    {
        log_info(
            "Voting for ",
            packet.meta.origin.to_string(),
            ".");

        RaftRPCData reply_data = 
        {
            success : true
        };

        PacketData data = packet.data;
        data.header.flags = data.header.flags | ACK;
        data.header.id.origin = local_node.get_address();
        memcpy(data.message_data, &reply_data, sizeof(reply_data));

        PacketMetadata meta = {
            transmission_uuid : UUID(""),
            origin : local_node.get_address(),
            destination : packet.meta.origin,
            message_length : sizeof(reply_data),
            expects_ack : 0,
            silent : 0
        };

        Packet p = {
            data : data,
            meta : meta
        };

        Node& destination = nodes.get_node(p.meta.destination);
        Connection& connection = connections.at(destination.get_id());
        connection.transmit(p);
    }

    void on_candidate()
    {
        candidate_timeout();
    }

    void candidate_timeout()
    {
        log_info("Candidate timed out; starting new election.");
        data.voted_for = nullptr;
        save_data();
        
        leader = nullptr;

        received_votes.clear();

        set_election_timer();
        send_request_vote();
        check_if_won_election();
    }

    void set_election_timer()
    {
        cancel_election_timer();
        int election_time = election_time_dis(rc_random::gen);
        timer_id = timer.add(election_time, election_timeout_handlers.at(state));
    }

    void cancel_election_timer()
    {
        if (timer_id != 0) timer.cancel(timer_id);
    }

    void send_request_vote()
    { 
        RaftRPCData rvo_data = 
        {
            success : 0
        };

        PacketData data;
        memset(&data, 0, sizeof(PacketData));
        data.header = {
            id : {
                origin : local_node.get_address(),
                msg_num : 0
            },
            fragment_num: 0,
            checksum: 0,
            flags: RVO,
            type : MessageType::RAFT
        };
        memcpy(data.message_data, &rvo_data, sizeof(rvo_data));

        PacketMetadata meta = {
            transmission_uuid : UUID(""),
            origin : local_node.get_address(),
            destination : {BROADCAST_ADDRESS, 0},
            message_length : sizeof(RaftRPCData),
            expects_ack : 0,
            silent : 0
        };

        Packet p = {
            data : data,
            meta : meta
        };

        // broadcast_connection.transmit(p, BroadcastType::BEB);
        // TODO: usar o broadcast connection aqui pra fazer um beb
        for (auto& [id, conn] : connections)
        {
            p.meta.destination = nodes.get_node(id).get_address();
            conn.transmit(p);
        }
    }

    void check_if_won_election()
    {
        unsigned int quota = get_quota();
        if (received_votes.size() >= quota)
        {
            log_info("Received quota, we are now the leader.");
            cancel_election_timer();
            change_state(LEADER);
        }
    }

    unsigned int get_quota()
    {
        unsigned int alive = 0;
        for (auto& [_, node] : nodes)
            if (node.is_alive()) alive++;

        return (alive / 2) + 1;
    }

    void candidate_receive(Packet packet)
    {
        if (packet.data.header.is_ldr())
        {
            follow(nodes.get_node(packet.meta.origin));
            return;
        }

        if (packet.data.header.is_rvo())
        {
            if (packet.data.header.is_ack())
            {
                RaftRPCData* rvo_data = reinterpret_cast<RaftRPCData*>(packet.data.message_data);
                if (rvo_data->success)
                {
                    log_debug(
                        "Received vote from ",
                        packet.meta.origin.to_string(),
                        ".");
                    received_votes.emplace(packet.meta.origin);
                    check_if_won_election();
                }
                return;
            }

            if (data.voted_for != nullptr)
            {
                log_debug("Received vote request, but we already voted for someone.");
                return;
            }

            if (should_grant_vote(packet.meta.origin))
            {
                send_vote(packet);
                set_election_timer();
            }
        }
    }

    void on_leader()
    {
        leader = &local_node;
        leader->set_leader(true);
    }

    void leader_receive(Packet packet)
    {
        if (packet.data.header.is_ldr())
        {
            if (packet.meta.origin != local_node.get_address()) follow(nodes.get_node(packet.meta.origin));
        }
    }

    void follow(Node& node)
    {
        log_info(
            "New leader is ",
            node.get_address().to_string(),
            "."
            );
        if (leader != nullptr) leader->set_leader(false);
        leader = &node;
        leader->set_leader(true);
        cancel_election_timer();
        change_state(FOLLOWER);
    }

    Observer<PacketReceived> obs_packet_received;

    void packet_received(const PacketReceived& event)
    {
        Packet& packet = event.packet;
        MessageType type = packet.data.header.get_message_type();

        if (type != MessageType::RAFT && type != MessageType::HEARTBEAT) return;

        if (!packet.silent())
        {
            log_trace(
            "Packet ",
            packet.to_string(PacketFormat::RECEIVED),
            " received by raft manager."
            );
        }

        // if (packet.data.header.is_ack()) pipeline.notify(PacketAckReceived(packet));

        packet_receive_handlers.at(state)(packet);
    }

    bool should_grant_vote(SocketAddress address)
    {
        for (auto &[id, node] : nodes)
        {
            if (!node.is_alive()) continue;
            if (strcmp(node.get_address().to_string().c_str(), address.to_string().c_str()) < 0)
            {
                log_debug("Not granting vote to ", address.to_string(), "; ", node.get_address().to_string()," has a higher priority.");
                return false;
            }
        }

        return true;
    }

public:
    RaftManager(std::map<std::string, Connection>& connections, NodeMap& nodes, Node& local_node, Pipeline& pipeline) : connections(connections), nodes(nodes), local_node(local_node), pipeline(pipeline), timer_id(0), election_time_dis(3000, 4000), leader(nullptr), data_filename(format("%s.raft", local_node.get_id().c_str())), state(FOLLOWER) {
        // TODO: o election_timeout_dis tem que ser definido com base no `alive`
        
        read_data();

        obs_packet_received.on(std::bind(&RaftManager::packet_received, this, _1));        
        pipeline.attach(obs_packet_received);

        set_election_timer();
    }
};

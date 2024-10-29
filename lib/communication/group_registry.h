#pragma once

#include <string>
#include <map>

#include "communication/connection.h"
#include "communication/broadcast_connection.h"
#include "core/node.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/buffer.h"
#include "core/event.h"
#include "raft/raft.h"
#include "utils/observer.h"

class Pipeline;

class GroupRegistry
{
    EventBus& event_bus;
    
    Observer<LeaderElected> obs_leader_elected;
    
    void leader_elected(const LeaderElected&);

    void update_leader_queue();

    void attach_observers();
public:
    GroupRegistry(std::string local_id, Config config, EventBus& event_bus);
    ~GroupRegistry();

    NodeMap &get_nodes();
    Node &get_local_node();

    bool has_connection(std::string id) {
        return connections.contains(id);
    }

    Connection& get_connection(SocketAddress address)
    {
        return get_connection(nodes.get_node(address));
    }
    Connection& get_connection(const Node& node)
    {
        return get_connection(node.get_id());
    }
    Connection& get_connection(std::string id)
    {
        return connections.at(id);
    }

    bool enqueue(Transmission& transmission);

    void update(std::string id);

    void establish_connections(
        Pipeline& pipeline,
        Buffer<Message> &application_buffer,
        Buffer<Message> &deliver_buffer,
        BufferSet<std::string> &connection_update_buffer,
        unsigned int alive
    );

private:
    std::string local_id;

    NodeMap nodes;
    std::map<std::string, Connection> connections;
    std::unique_ptr<BroadcastConnection> broadcast_connection;
    std::unique_ptr<RaftManager> raft;

    std::queue<Transmission*> leader_transmissions;

    void read_nodes_from_configuration(std::string local_id, Config config);
};

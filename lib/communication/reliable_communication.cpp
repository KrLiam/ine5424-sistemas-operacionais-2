#include "communication/reliable_communication.h"
#include "core/format.h"

ReliableCommunication::ReliableCommunication(std::string local_id, std::size_t buffer_size)
    : buffer_size(buffer_size),
      nodes(create_nodes(local_id))
{
    const Node& local_node = get_node(local_id);
    int port = local_node.get_address().port;
    
    channel = std::make_unique<Channel>(port);
}

void ReliableCommunication::send(std::string id, char* m) {
    Node destination = get_node(id);
    channel->send(destination.get_address(), m, buffer_size);
}

std::size_t ReliableCommunication::receive(char* m) {
    return channel->receive(m, buffer_size);
}

const Node& ReliableCommunication::get_node(std::string id) {
    // talvez dava pra usar um mapa de id:nó para os nodes
    for (Node& node : nodes) {
        if (node.get_id() == id) return node;
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

const std::vector<Node>& ReliableCommunication::get_nodes() {
    return nodes;
}

std::vector<Node> ReliableCommunication::create_nodes(std::string local_id) {
    std::vector<Node> _nodes;

    Config config = ConfigReader::parse_file("nodes.conf");
    for(NodeConfig node_config : config.nodes) {;
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        _nodes.push_back(node);
    }

    return _nodes;
}
#include "reliablecommunication.h"
#include "format.h"

ReliableCommunication::ReliableCommunication() {
}

ReliableCommunication::~ReliableCommunication() {
}

void ReliableCommunication::initialize(std::string local_id, std::size_t _buffer_size) {
    buffer_size = _buffer_size;
    nodes = create_nodes(local_id);

    channel = new Channel();
    channel->initialize(get_node(local_id).get_address().port);
}

void ReliableCommunication::deinitialize() {
    channel->deinitialize();
    delete channel;
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
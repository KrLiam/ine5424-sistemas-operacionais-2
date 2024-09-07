#include "communication/reliable_communication.h"

static void receiver_thread(ReliableCommunication *manager)
{
    log_info("Initialized receiver thread.");
    manager->listen_receive();
    log_info("Closing reicever thread.");
}

static void sender_thread(ReliableCommunication *manager)
{
    log_info("Initialized sender thread.");
    manager->listen_send();
    log_info("Closing sender thread.");
}

ReliableCommunication::ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size)
    : local_id(_local_id), user_buffer_size(_user_buffer_size), nodes(create_nodes(_local_id))
{
    const Node &local_node = get_node(local_id);
    int port = local_node.get_address().port;

    channel = std::make_unique<Channel>(port);

    std::thread listener_thread_obj(receiver_thread, this);
    listener_thread_obj.detach();
    std::thread sender_thread_obj(sender_thread, this);
    sender_thread_obj.detach();
}

void ReliableCommunication::listen_receive()
{
    while (true)
    {
        Segment segment = channel->receive();
        if (!receive_buffer.produce(segment, false))
        {
            continue;
        }
    }
}

void ReliableCommunication::listen_send()
{
    while (true)
    {
        Segment segment = send_buffer.consume();
        channel->send(segment);
    }
}

void ReliableCommunication::send(std::string id, char *m)
{
    Node origin = get_node(local_id);
    Node destination = get_node(id);

    Packet packet = Packet::from(m); // arrumar pra fragmentacao dps
    Segment segment = Segment{
        origin : origin.get_address(),
        destination : destination.get_address(),
        packet : packet
    };

    send_buffer.produce(segment, true);
}

Segment ReliableCommunication::receive(char *m)
{
    Segment segment = receive_buffer.consume();
    strncpy(m, segment.packet.data, Packet::DATA_SIZE);
    return segment;
}

const Node &ReliableCommunication::get_node(std::string id)
{
    // talvez dava pra usar um mapa de id:nó para os nodes
    for (Node &node : nodes)
    {
        if (node.get_id() == id)
            return node;
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

const std::vector<Node> &ReliableCommunication::get_nodes()
{
    return nodes;
}

std::vector<Node> ReliableCommunication::create_nodes(std::string local_id)
{
    std::vector<Node> _nodes;

    Config config = ConfigReader::parse_file("nodes.conf");
    for (NodeConfig node_config : config.nodes)
    {
        ;
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        _nodes.push_back(node);
    }

    return _nodes;
}

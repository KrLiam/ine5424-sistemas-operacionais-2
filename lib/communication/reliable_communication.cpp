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
    : local_id(_local_id), user_buffer_size(_user_buffer_size), nodes(create_nodes())
{
    const Node &local_node = get_node(local_id);
    channel = std::make_unique<Channel>(local_node.get_address());

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
    Node origin = get_local_node();
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
    std::map<std::string, Node>::iterator iterator = nodes.find(id);
    if (iterator != nodes.end())
    {
        return iterator->second;
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

const std::map<std::string, Node> &ReliableCommunication::get_nodes()
{
    return nodes;
}

const Node &ReliableCommunication::get_local_node()
{
    return get_node(local_id);
}

std::map<std::string, Node> ReliableCommunication::create_nodes()
{
    std::map<std::string, Node> _nodes;

    Config config = ConfigReader::parse_file("nodes.conf");
    for (NodeConfig node_config : config.nodes)
    {
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        _nodes.emplace(node.get_id(), node);
    }

    return _nodes;
}

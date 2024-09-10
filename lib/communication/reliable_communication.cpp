#include "communication/reliable_communication.h"

ReliableCommunication::ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size)
    : local_id(_local_id), user_buffer_size(_user_buffer_size), nodes(create_nodes())
{
    create_connections();

    const Node &local_node = get_node(local_id);
    channel = new Channel(local_node.get_address());

    pipeline = new Pipeline(this, channel);
}

ReliableCommunication::~ReliableCommunication()
{
    delete channel;
    delete pipeline;
}

void ReliableCommunication::send(std::string id, char *m)
{
    Message message = {
        origin : get_local_node().get_address(),
        destination : get_node(id).get_address(),
        length : user_buffer_size
    };
    strncpy(message.data, m, user_buffer_size);

    char msg[sizeof(Message)];
    memcpy(msg, &message, sizeof(Message));
    pipeline->send_to(PipelineStep::PROCESS_LAYER, msg);
}

Message ReliableCommunication::receive(char *m)
{
    Message message = receive_buffer.consume();

    if (user_buffer_size >= message.length)
    {
        strncpy(m, message.data, message.length);
        return message;
    }

    throw std::runtime_error("Provided buffer is smaller than the message.");
    /*log_warn("Buffer size defined by the user [", user_buffer_size , "] is smaller ",
    "than the received message's size [", message.length, "]; we will split this message into multiple ones.");
    int required_parts = ceil((double) message.length / user_buffer_size);
    for (int i = 0; i < required_parts; i++)
    {
        Message part = {
            origin: message.origin,
            destination: message.destination,
            type: message.type,
            part: i,
            has_more_parts: i != required_parts - 1
        };
        part.length = i * user_buffer_size;
        strncpy(part.data, &message.data[i * user_buffer_size], part.length);
        receive_buffer.produce(part);
    }

    return receive(m); // problema: perde a ordem */
}

void ReliableCommunication::produce(Message message)
{
    send_buffer.produce(message);
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
const Node &ReliableCommunication::get_node(SocketAddress address)
{
    for (auto &[id, node] : nodes)
    {
        if (address == node.get_address())
        {
            return node;
        }
    }
    throw std::invalid_argument(format("Node with address %s not found.", address.to_string().c_str()));
}
const std::map<std::string, Node> &ReliableCommunication::get_nodes()
{
    return nodes;
}
const Node &ReliableCommunication::get_local_node()
{
    return get_node(local_id);
}

Connection *ReliableCommunication::get_connection(std::string id)
{
    return connections.at(id);
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

void ReliableCommunication::create_connections()
{
    // imeplementar handshake e fazer as conexões serem dinamicas
    for (auto &[id, node] : nodes)
    {
        Connection *c = new Connection();
        connections.emplace(id, c);
    }
}
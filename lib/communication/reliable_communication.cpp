#include "communication/reliable_communication.h"

static void management_thread(ReliableCommunication* manager)
{
    log_info("Initialized management thread.");
    manager->run();
    log_info("Closing management thread.");
}

ReliableCommunication::ReliableCommunication(std::string local_id, std::size_t buffer_size)
    : buffer_size(buffer_size),
      nodes(create_nodes(local_id))
{
    const Node& local_node = get_node(local_id);
    int port = local_node.get_address().port;
    
    channel = std::make_unique<Channel>(port);

    std::thread management_thread_obj(management_thread, this);
    management_thread_obj.detach();
}

void ReliableCommunication::run()
{
    char message[MESSAGE_SIZE];

    while (true)
    {

        channel->receive(message, MESSAGE_SIZE);
        if (!receive_producer.try_acquire())
        {
            continue;
        }
        strncpy(&receive_buffer[receive_buffer_end], message, MESSAGE_SIZE);
        log_debug("Wrote ", MESSAGE_SIZE, " bytes on receive buffer starting at ", receive_buffer_end, ".");
        receive_buffer_end = (receive_buffer_end + MESSAGE_SIZE) % (INTERMEDIARY_BUFFER_SIZE);
        receive_consumer.release();
    }
}

void ReliableCommunication::send(std::string id, char* m) {
    // TODO: buffer de envio
    Node destination = get_node(id);
    channel->send(destination.get_address(), m, buffer_size);
}

std::size_t ReliableCommunication::receive(char* m) {
    // TODO: ler tamanho variável
    receive_consumer.acquire();
    strncpy(m, &receive_buffer[receive_buffer_start], MESSAGE_SIZE);
    log_debug("Read ", MESSAGE_SIZE, " bytes from receive buffer starting at ", receive_buffer_start, ".");
    receive_buffer_start = (receive_buffer_start + MESSAGE_SIZE) % (INTERMEDIARY_BUFFER_SIZE);
    receive_producer.release();
    return MESSAGE_SIZE;
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

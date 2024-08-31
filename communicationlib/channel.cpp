#include "channel.h"

Channel::Channel()
{
}

Channel::~Channel()
{
    for (const auto& [key, node] : nodes)
    {
        if (node.get_socket() != -1) {
            // close(node.get_socket());
            std::cout << std::format("Closed socket for node {}.", node.to_string()) << std::endl;
        }
        // talvez uma close_socket_for_node(Node node);
    }
}

void Channel::init(std::string local_id, std::vector<Node> nodes)
{
    this->local_id = local_id;
    
    for (const Node &node : nodes)
    {
        initialize_socket_for_node(node);
    }
}

void Channel::initialize_socket_for_node(Node node)
{
    auto& config = node.get_config();

    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        std::cout << std::format("Unable to create socket for node {}.", node.to_string()) << std::endl;
        return;
    }

    sockaddr_in socket_address;
    memset(&socket_address, 0, sizeof(sockaddr_in));
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(config.address.port);
    socket_address.sin_addr.s_addr =
        node.is_remote() ?
        inet_addr(config.address.address.to_string().c_str()) :
        INADDR_ANY;

    int bind_result = bind(
        socket_descriptor, (const struct sockaddr *)&socket_address, sizeof(socket_address));
    if (bind_result < 0)
    {
        std::cout << std::format("Unable to bind socket for node {}.", node.to_string()) << std::endl;
        return;
    }

    node.set_socket(socket_descriptor);
    node.set_socket_address(socket_address);
    nodes.emplace(config.id, node);
    std::cout << std::format("Socket for node {} initialized successfully.", node.to_string()) << std::endl; // TODO: criar metodo generico pra fazer isso
}

void Channel::send(Node node, char *m, std::size_t size)
{
    if (node.get_socket() == -1) {
        std::cout << std::format("No socket for node {} exists.", node.to_string()) << std::endl;
        return;
    }

    sockaddr_in socket_address = node.get_socket_address();
    int bytes_sent = sendto(node.get_socket(), m, size, 0, (struct sockaddr *)&socket_address, sizeof(socket_address));
    if (bytes_sent < 0)
    {
        std::cout << std::format("Unable to send message to node {}.", node.to_string()) << std::endl;
        return;
    }
    std::cout << std::format("Sent {} bytes to node {}.", bytes_sent, node.to_string()) << std::endl; // TODO: criar metodo generico pra fazer isso
}

std::size_t Channel::receive(char *m)
{
    // int bytes_received = recvfrom(local_socket, buff, 1024, 0, (struct sockaddr*)&cliAddr, &cliAddrLen);
    // if (readStatus < 0) { 
    //     perror("reading error...\n");
    //     close(serSockDes);
    //     exit(-1);
    // }

    // std::cout.write(buff, readStatus);
    // std::cout << std::endl;
    return 0;
}
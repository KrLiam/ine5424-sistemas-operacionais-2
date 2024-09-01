#include "channel.h"

Channel::Channel()
{
}

Channel::~Channel()
{
}

void Channel::initialize(int port)
{
    open_socket(port);
}

void Channel::deinitialize() {
    close_socket();
}

void Channel::open_socket(int port)
{
    socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        std::cout << "Unable to create a socket." << std::endl;
        return;
    }

    memset(&in_address, 0, sizeof(sockaddr_in));
    in_address.sin_family = AF_INET;
    in_address.sin_port = htons(port);
    in_address.sin_addr.s_addr = INADDR_ANY;
    
    memset(&out_address, 0, sizeof(sockaddr_in));
    out_address.sin_family = AF_INET;

    int bind_result = bind(socket_descriptor, (const struct sockaddr *)&in_address, sizeof(in_address));
    if (bind_result < 0)
    {
        std::cout << std::format("Unable to bind socket on port {}.", port) << std::endl;
        return;
    }

    std::cout << std::format("Successfully binded socket to port {}.", port) << std::endl; // TODO: criar metodo generico pra fazer isso
}

void Channel::close_socket() {
    close(socket_descriptor);
}

void Channel::send(SocketAddress endpoint, char *m, std::size_t size)
{
    out_address.sin_port = htons(endpoint.port);
    out_address.sin_addr.s_addr = inet_addr(endpoint.address.to_string().c_str());

    int bytes_sent = sendto(socket_descriptor, m, size, 0, (struct sockaddr *)&out_address, sizeof(out_address));
    if (bytes_sent < 0)
    {
        std::cout << std::format("Unable to send message to {}.", endpoint.to_string()) << std::endl;
        return;
    }
    std::cout << std::format("Sent {} bytes to {}.", bytes_sent, endpoint.to_string()) << std::endl; // TODO: criar metodo generico pra fazer isso
}

std::size_t Channel::receive(char *m, std::size_t size)
{
    socklen_t len = sizeof(in_address);
    std::size_t bytes_received = recvfrom(socket_descriptor, m, size, 0, (struct sockaddr*)&in_address, &len);
    
    // passar isso para uma função q dá uma struct com o size recebido e outras informações
    char remote_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in_address.sin_addr, remote_address, INET_ADDRSTRLEN);
    int remote_port = ntohs(in_address.sin_port);

    std::cout << std::format("Received {} bytes from {}:{}.", bytes_received, remote_address, remote_port) << std::endl;
    return bytes_received;
}

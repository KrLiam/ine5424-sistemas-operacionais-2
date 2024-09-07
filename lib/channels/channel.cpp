#include "channels/channel.h"

Channel::Channel(SocketAddress local_address) : address(local_address)
{
    open_socket();
}

Channel::~Channel()
{
    if (socket_descriptor != -1)
        close_socket();
}

void Channel::open_socket()
{
    socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        log_error("Unable to create a socket.");
        return;
    }

    memset(&in_address, 0, sizeof(sockaddr_in));
    in_address.sin_family = AF_INET;
    in_address.sin_port = htons(address.port);
    in_address.sin_addr.s_addr = INADDR_ANY;

    memset(&out_address, 0, sizeof(sockaddr_in));
    out_address.sin_family = AF_INET;

    int bind_result = bind(socket_descriptor, (const struct sockaddr *)&in_address, sizeof(in_address));
    if (bind_result < 0)
    {
        log_error("Unable to bind socket on port ", address.port, ".");
        return;
    }
    log_debug("Successfully binded socket to port ", address.port, ".");
}

void Channel::close_socket()
{
    close(socket_descriptor);
    log_debug("Closed channel");
}

void Channel::send(Segment segment)
{
    SocketAddress destination = segment.destination;
    out_address.sin_port = htons(destination.port);
    out_address.sin_addr.s_addr = inet_addr(destination.address.to_string().c_str());

    int bytes_sent = sendto(
        socket_descriptor,
        (char *)&segment.packet,
        Packet::SIZE,
        0,
        (struct sockaddr *)&out_address,
        sizeof(out_address));
    if (bytes_sent < 0)
    {
        log_warn("Unable to send message to ", destination.to_string(), ".");
        return;
    }
    log_info("Sent ", bytes_sent, " bytes to ", destination.to_string(), ".");
}

Segment Channel::receive()
{
    Segment segment;
    socklen_t in_address_len = sizeof(in_address);

    log_debug("Waiting to receive data.");
    std::size_t bytes_received = recvfrom(
        socket_descriptor,
        (char *)&segment.packet,
        sizeof(Packet),
        0,
        (struct sockaddr *)&in_address,
        &in_address_len);
    
    SocketAddress origin = SocketAddress::from(in_address);
    log_debug("Received ", bytes_received, " bytes from ", origin.address.to_string(), ":", origin.port, ".");

    segment.origin = origin;
    segment.destination = address;
    segment.data_size = bytes_received - sizeof(PacketHeader);

    return segment;
}

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
    shutdown(socket_descriptor, SHUT_RDWR);
    close(socket_descriptor);
    log_debug("Closed channel");
}

void Channel::send(Packet packet)
{
    const PacketHeader& header = packet.data.header;

    SocketAddress destination = packet.meta.destination;
    out_address.sin_port = htons(destination.port);
    out_address.sin_addr.s_addr = inet_addr(destination.address.to_string().c_str());

    int bytes_sent = sendto(
        socket_descriptor,
        (char *)&packet.data,
        packet.meta.message_length + sizeof(header),
        0,
        (struct sockaddr *)&out_address,
        sizeof(out_address));
    if (bytes_sent < 0)
    {
        log_warn("Unable to send message to ", destination.to_string(), ".");
        return;
    }
    log_info("Sent packet ", header.msg_num, "/", header.fragment_num, " (", bytes_sent, " bytes) to ", destination.to_string(), ".");
}

Packet Channel::receive()
{
    Packet packet;
    socklen_t in_address_len = sizeof(in_address);

    log_debug("Waiting to receive data.");
    int bytes_received = recvfrom(
        socket_descriptor,
        (char *)&packet.data,
        sizeof(PacketData),
        0,
        (struct sockaddr *)&in_address,
        &in_address_len);

    if (bytes_received <= 0 || errno != 0)
        return Packet{};

    SocketAddress origin = SocketAddress::from(in_address);

    const PacketHeader& header = packet.data.header;
    log_info("Received packet ", header.msg_num, "/", header.fragment_num, " (", bytes_received, " bytes) from ", origin.to_string(), ".");

    packet.meta.origin = origin;
    packet.meta.destination = address;
    // packet.meta.time = now();
    packet.meta.message_length = bytes_received - sizeof(PacketHeader);

    return packet;
}

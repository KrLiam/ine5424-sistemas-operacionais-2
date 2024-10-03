#include "channels/channel.h"

Channel::Channel(const SocketAddress local_address) : address(local_address)
{
    open_socket();
}

Channel::~Channel()
{
    if (socket_descriptor != -1)
        close_socket();
}

void Channel::open_socket() {
    socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0) {
        throw std::runtime_error("Unable to create a socket.");
    }

    memset(&in_address, 0, sizeof(sockaddr_in));
    in_address.sin_family = AF_INET;
    in_address.sin_port = htons(address.port);
    in_address.sin_addr.s_addr = INADDR_ANY;

    memset(&out_address, 0, sizeof(sockaddr_in));
    out_address.sin_family = AF_INET;

    const int bind_result = bind(
        socket_descriptor,
        reinterpret_cast<const struct sockaddr*>(&in_address),
        sizeof(in_address)
        );
    if (bind_result < 0) {
        throw port_in_use_error(
            format("Port %d is already in use. This is likely not an issue with the library.", address.port)
        );
    }

    log_debug("Successfully binded socket to port ", address.port, ".");
}


void Channel::close_socket() const
{
    close(socket_descriptor);
    log_trace("Closed channel");
}

void Channel::shutdown_socket() const
{
    shutdown(socket_descriptor, SHUT_RDWR);
}

void Channel::send(Packet packet)
{
    [[maybe_unused]] const PacketHeader& header = packet.data.header;
    const SocketAddress destination = packet.meta.destination;

    if (destination == BROADCAST_ADDRESS) return;

    send_mutex.lock();

    out_address.sin_port = htons(destination.port);
    out_address.sin_addr.s_addr = inet_addr(destination.address.to_string().c_str());

    int bytes_sent = sendto(
        socket_descriptor,
        (char *)&packet.data,
        packet.meta.message_length + sizeof(header),
        0,
        reinterpret_cast<struct sockaddr*>(&out_address),
        sizeof(out_address));
    
    send_mutex.unlock();

    if (bytes_sent < 0)
    {
        log_warn("Unable to send message to ", destination.to_string(), ".");
        return;
    }
    log_info("Sent packet ", packet.to_string(PacketFormat::SENT), " (", bytes_sent, " bytes).");
}

Packet Channel::receive()
{
    Packet packet;
    socklen_t in_address_len = sizeof(in_address);

    log_trace("Waiting to receive data.");
    int bytes_received = recvfrom(
        socket_descriptor,
        (char *)&packet.data,
        sizeof(PacketData),
        0,
        reinterpret_cast<struct sockaddr*>(&in_address),
        &in_address_len);

    if (bytes_received <= 0 || errno != 0)
        throw std::runtime_error("Socket closed.");

    packet.meta.destination = address;
    packet.meta.message_length = bytes_received - sizeof(PacketHeader);

    return packet;
}

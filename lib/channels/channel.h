#pragma once

#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/log.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/node.h"

class Channel
{
public:
    explicit Channel(SocketAddress local_address);
    ~Channel();

    void send(Packet packet);
    Packet receive();

    void shutdown_socket() const;
private:
    SocketAddress address;
    Packet buffer;
    std::mutex send_mutex;

    int socket_descriptor = -1;
    sockaddr_in in_address{};
    sockaddr_in out_address{};

    void open_socket();
    void close_socket() const;
};

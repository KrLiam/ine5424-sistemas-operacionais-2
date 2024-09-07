#pragma once

#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/log.h"
#include "core/segment.h"
#include "core/node.h"

class Channel
{
public:
    Channel(SocketAddress local_address);
    ~Channel();

    void send(Segment segment);
    Segment receive();

private:
    SocketAddress address;
    Packet buffer;

    int socket_descriptor = -1;
    sockaddr_in in_address;
    sockaddr_in out_address;

    void open_socket();
    void close_socket();
};

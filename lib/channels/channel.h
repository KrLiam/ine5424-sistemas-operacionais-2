#pragma once

#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/log.h"
#include "core/node.h"

class Channel
{
public:
    Channel(int port);
    ~Channel();

    void send(SocketAddress endpoint, char *m, std::size_t size);
    std::size_t receive(char *m, std::size_t size);


private:
    std::string local_id;

    int socket_descriptor = -1;
    sockaddr_in in_address;
    sockaddr_in out_address;

    void open_socket(int port);
    void close_socket();
};

#pragma once

#include <vector>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/config.h"
#include "core/log.h"
#include "communication/node.h"

class Channel
{
public:
    Channel();
    ~Channel();

    void initialize(int port);
    void deinitialize();

    void send(SocketAddress endpoint, char *m, std::size_t size);
    std::size_t receive(char *m, std::size_t size);


private:
    std::string local_id;

    int socket_descriptor;
    sockaddr_in in_address;
    sockaddr_in out_address;

    void open_socket(int port);
    void close_socket();
};

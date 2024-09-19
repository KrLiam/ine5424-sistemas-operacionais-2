#pragma once

#include <condition_variable>
#include <mutex>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <exception>

#include "utils/config.h"
#include "utils/format.h"
#include "core/message.h"
#include "core/packet.h"
#include "utils/log.h"
#include "core/node.h"
#include "core/buffer.h"
#include "utils/date.h"
#include "core/constants.h"

using namespace std::placeholders;

class Pipeline;

enum ConnectionState
{
    CLOSED = 0,
    SYN_SENT = 1,
    SYN_RECEIVED = 2,
    ESTABLISHED = 3,
    FIN_WAIT = 4,
    LAST_ACK = 5
};

class Connection
{
private:
    Pipeline &pipeline;
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer;

    Node local_node;
    Node remote_node;

    uint32_t next_number = 0;
    uint32_t expected_number = 0;

    ConnectionState state = CLOSED;
    std::condition_variable state_change;
    std::mutex mutex;

    Timer timer{};
    int connect_timer = -1;

    const std::map<ConnectionState, std::function<void(Packet)>> packet_receive_handlers = {
        {ESTABLISHED, std::bind(&Connection::established, this, _1)},
        {SYN_SENT, std::bind(&Connection::syn_sent, this, _1)},
        {SYN_RECEIVED, std::bind(&Connection::syn_received, this, _1)},
        {CLOSED, std::bind(&Connection::closed, this, _1)},
        {FIN_WAIT, std::bind(&Connection::fin_wait, this, _1)},
        {LAST_ACK, std::bind(&Connection::last_ack, this, _1)}};

    const std::map<ConnectionState, std::string> state_names = {
        {ESTABLISHED, "established"},
        {SYN_SENT, "syn_sent"},
        {SYN_RECEIVED, "syn_received"},
        {CLOSED, "closed"},
        {FIN_WAIT, "fin_wait"},
        {LAST_ACK, "last_ack"}};

    enum
    {
        ACK = 0x01,
        RST = 0x02,
        SYN = 0x04,
        FIN = 0x08,
    };

    void connect();
    bool disconnect();

    void closed(Packet p);

    void syn_sent(Packet p);

    void syn_received(Packet p);

    void established(Packet p);

    void fin_wait(Packet p);

    void last_ack(Packet p);

    void send_flag(unsigned char flags);

    void send_ack(Packet packet);

    bool close_on_rst(Packet p);

    bool rst_on_syn(Packet p);

    void connection_timeout();

    uint32_t new_message_number();

    void reset_message_numbers();

    std::string get_current_state_name();

    void change_state(ConnectionState new_state);

public:
    Connection(Pipeline &pipeline, Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer, Node local_node, Node remote_node) : pipeline(pipeline), application_buffer(application_buffer), local_node(local_node), remote_node(remote_node) {}

    void send(Message message);
    void send(Packet packet);

    void receive(Packet packet);
    void receive(Message message);
};

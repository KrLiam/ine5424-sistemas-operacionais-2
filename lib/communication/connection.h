#pragma once

#include <condition_variable>
#include <mutex>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <vector>
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
#include "core/event.h"
#include "utils/observer.h"
#include "communication/transmission.h"

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
    Buffer<Message> &application_buffer;

    Node local_node;
    Node remote_node;

    ConnectionState state = CLOSED;
    std::condition_variable state_change;

    Transmission* active_transmission = nullptr;
    std::vector<Transmission*> transmissions;
    // Atualmente só tá sendo enviado packet sem esperar ACK, a ideia é fazer com que a mesma lógica de
    // aguardar o envio seja possível para os pacotes da lib
    // Para isso, vai ser necessário adaptar para ter um TransmissionQueue por ato de send
    // ou um TransmissionQueue só suportar mensagem + pacotes não relacionados
    std::vector<Packet> packets_to_send;
    BufferSet<std::string>& connection_update_buffer;

    uint32_t next_number = 0;
    uint32_t expected_number = 0;

    Timer timer{};
    int handshake_timer_id = -1;

    std::mutex mutex;
    std::mutex mutex_transmissions;
    std::mutex mutex_packets;


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

    enum // TODO: mover isso para o Packet e fazer ele mesmo definir as flags
    {
        ACK = 0x01,
        RST = 0x02,
        SYN = 0x04,
        FIN = 0x08,
    };

    void transmit(Packet);

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

    void set_timeout();
    void connection_timeout();

    uint32_t new_message_number();
    void reset_message_numbers();

    void cancel_transmissions();
    void complete_transmission();

    std::string get_current_state_name();
    void change_state(ConnectionState new_state);

    void observe_pipeline();

    Observer<MessageDefragmentationIsComplete> obs_message_defragmentation_is_complete;
    Observer<TransmissionComplete> obs_transmission_complete;
    Observer<TransmissionFail> obs_transmission_fail;
    
    void message_defragmentation_is_complete(const MessageDefragmentationIsComplete& event);
    void transmission_complete(const TransmissionComplete& event);
    void transmission_fail(const TransmissionFail& event);

public:
    Connection(
        Node local_node,
        Node remote_node,
        Pipeline &pipeline,
        Buffer<Message> &application_buffer,
        BufferSet<std::string>& connection_update_buffer
    );

    bool enqueue(Transmission& transmission);

    void request_update();

    void update();

    void send(Message message);
    void send(Packet packet);

    void receive(Packet packet);
    void receive(Message message);
};

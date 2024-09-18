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
    uint32_t unacknowledged_number = 0;
    uint32_t expected_number = 0;

    ConnectionState state = ConnectionState::CLOSED;
    std::condition_variable state_change;
    std::mutex mutex;

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

    bool connect()
    {
        if (state == ConnectionState::ESTABLISHED)
            return true;

        log_debug("connect: sending SYN.");
        set_message_numbers(0);
        change_state(ConnectionState::SYN_SENT);
        send_flag(SYN);

        std::unique_lock lock(mutex);
        while (state != ConnectionState::ESTABLISHED)
            state_change.wait(lock); // TODO: timeout

        log_debug("connect: connection established successfully.");
        return true;
    }
    bool disconnect()
    {
        if (state == ConnectionState::CLOSED)
            return true;

        log_debug("disconnect: sending FIN.");
        set_message_numbers(0);
        change_state(ConnectionState::FIN_WAIT);
        send_flag(FIN);

        std::unique_lock lock(mutex);
        while (state != ConnectionState::CLOSED)
            state_change.wait(lock); // TODO: timeout

        log_debug("disconnect: connection closed successfully.");
        return true;
    }

    void closed(Packet p)
    {
        if (p.data.header.is_syn() && !p.data.header.is_ack())
        {
            log_debug("closed: received SYN; sending SYN+ACK.");
            change_state(ConnectionState::SYN_RECEIVED);
            send_flag(SYN | ACK);
        }

        /* pro reschedule: também tem q fazer alterar o número dos pacotes no transmissionlayer
        // atualmente, dá problema caso B tenha enviado 1 mensagem pra A, A cai, B tenta enviar a 2a
        // da pra resolver isso tanto por reschedule quanto por timeout
        {
        if (p.data.header.msg_num != 0)
            log_warn("closed: received message packet.");
            Packet p = empty_packet();
            p.data.header.syn = 1; // Dá pra simplificar essa parte
            change_state(ConnectionState::SYN_SENT);
            send(p);
            return;
        }*/
    }

    void syn_sent(Packet p)
    {
        if (p.data.header.is_syn())
        {
            if (p.data.header.is_ack())
            {
                set_message_numbers(1);
                log_debug("syn_sent: received SYN+ACK; sending ACK.");
                log_info("syn_sent: connection established.");
                change_state(ConnectionState::ESTABLISHED);
                send_flag(ACK);
                return;
            }
            log_debug("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");
            change_state(ConnectionState::SYN_RECEIVED);
            send_flag(SYN | ACK);
            return;
        }

        if (p.data.header.is_rst())
        {
            log_debug("syn_sent: received RST; sending SYN.");
            send_flag(SYN);
            return;
        }

        log_debug("syn_sent: received unexpected packet; closing connection.");
        change_state(ConnectionState::CLOSED);
    }

    void syn_received(Packet p)
    {
        if (close_on_rst(p))
            return;

        if (p.data.header.is_ack())
        {
            set_message_numbers(1);
            log_debug("syn_received: received ACK.");
            log_info("syn_received: connection established.");
            change_state(ConnectionState::ESTABLISHED);
            return;
        }

        if (rst_on_syn(p))
            return;
    }

    void established(Packet p);

    void fin_wait(Packet p)
    {
        if (close_on_rst(p))
            return;
        if (rst_on_syn(p))
            return;

        if (p.data.header.is_fin())
        {
            if (p.data.header.is_ack())
            {
                log_debug("fin_wait: received FIN+ACK; closing and sending ACK.");
                change_state(ConnectionState::CLOSED);
                send_flag(ACK);
                return;
            }
            log_debug("fin_wait: simultaneous termination; transitioning to last_ack and sending ACK.");
            change_state(ConnectionState::LAST_ACK);
            send_flag(ACK);
        }
    }

    void last_ack(Packet p)
    {
        // graantir q o numero de seq é 0

        if (close_on_rst(p))
            return;
        if (rst_on_syn(p))
            return;

        if (p.data.header.is_ack())
        {
            log_debug("last_ack: received ACK.");
            log_info("last_ack: connection closed.");
            change_state(ConnectionState::CLOSED);
        }
    }

    void send_flag(unsigned char flags)
    {
        PacketHeader header;
        memset(&header, 0, sizeof(PacketHeader));
        header.msg_num = 0;
        header.type = MessageType::CONTROL;
        memcpy(reinterpret_cast<unsigned char *>(&header) + 12, &flags, 1);

        PacketData data;
        memset(&data, 0, sizeof(PacketData));
        data.header = header;

        Packet packet;
        memset(&packet, 0, sizeof(Packet));
        packet.meta.origin = local_node.get_address();
        packet.meta.destination = remote_node.get_address();
        packet.data = data;

        send(packet);
    }

    void send_ack(Packet packet)
    {
        PacketData data;
        data.header = {// TODO: Definir corretamente checksum, window, e reserved.
                       msg_num : packet.data.header.msg_num,
                       fragment_num : packet.data.header.fragment_num,
                       checksum : 0,
                       window : 0,
                       ack : 1,
                       rst : 0,
                       syn : 0,
                       fin : 0,
                       extra : 0,
                       more_fragments : 0,
                       type : MessageType::CONTROL,
                       reserved : 0
        };
        PacketMetadata meta = {
            origin : local_node.get_address(),
            destination : remote_node.get_address(),
            time : 0,
            message_length : 0
        };
        Packet ack_packet = {
            data : data,
            meta : meta
        };
        send(ack_packet);
    }

    bool close_on_rst(Packet p)
    {
        if (p.data.header.is_rst())
        {
            log_debug(get_current_state_name(), ": received RST.");
            log_info(get_current_state_name(), ": connection closed.");
            change_state(ConnectionState::CLOSED);
            return true;
        }
        return false;
    }

    bool rst_on_syn(Packet p)
    {
        if (p.data.header.is_syn())
        {
            log_debug(get_current_state_name(), ": received SYN; sending RST.");
            log_info(get_current_state_name(), ": connection closed.");
            change_state(ConnectionState::CLOSED);
            send_flag(RST);
            return true;
        }
        return false;
    }

    uint32_t new_message_number()
    {
        return next_number++;
    }

    void set_message_numbers(uint32_t number)
    {
        expected_number = number;
        unacknowledged_number = number;
        next_number = number;
    }

    std::string get_current_state_name()
    {
        return state_names.at(state);
    }

    void change_state(ConnectionState new_state)
    {
        if (state == new_state)
            return;
        state = new_state;
        state_change.notify_all();
    }

public:
    Connection(Pipeline &pipeline, Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer, Node local_node, Node remote_node) : pipeline(pipeline), application_buffer(application_buffer), local_node(local_node), remote_node(remote_node) {}

    void send(Message message);
    void send(Packet packet);

    void receive(Packet packet)
    {
        packet_receive_handlers.at(state)(packet);
    }
    void receive(Message message)
    {
        if (state != ConnectionState::ESTABLISHED)
        {
            log_warn("Connection is not established; dropping message ", message.to_string(), ".");
            return;
        }

        if (message.number < expected_number)
        {
            log_warn("Message ", message.to_string(), " was already received; dropping it.");
            return;
        }
        expected_number++;
        
        application_buffer.produce(message);
    }
};

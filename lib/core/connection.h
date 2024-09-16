#pragma once

#include <condition_variable>
#include <mutex>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>

#include "utils/config.h"
#include "utils/format.h"
#include "core/segment.h"
#include "utils/log.h"
#include "core/node.h"

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
    Pipeline &pipeline;

    Node local_node;
    Node remote_node;

    uint32_t remote_expected_message_number = 0;
    uint32_t local_unacknowlodged_message_number = 0;
    uint32_t local_next_message_number = 0;

    ConnectionState state = ConnectionState::CLOSED;
    std::condition_variable state_change;
    std::mutex mutex;

public:
    Connection(Pipeline &pipeline, Node local_node, Node remote_node) : pipeline(pipeline), local_node(local_node), remote_node(remote_node) {}

    void send(Message message);
    void send(Packet packet);

    void receive(Packet packet)
    {
        switch (state)
        {
        case ConnectionState::ESTABLISHED:
            established(packet);
            break;

        case ConnectionState::SYN_SENT:
            syn_sent(packet);
            break;

        case ConnectionState::SYN_RECEIVED:
            syn_received(packet);
            break;

        case ConnectionState::CLOSED:
            closed(packet);
            break;

        case ConnectionState::FIN_WAIT:
            fin_wait(packet);
            break;

        case ConnectionState::LAST_ACK:
            last_ack(packet);
            break;
        }
    }

    void receive(Message message)
    {
        if (state != ConnectionState::ESTABLISHED)
        {
            log_warn("conexao nao estabelecida, dropando.");
            return;
        }

        if (message.number < remote_expected_message_number)
        {
            log_warn("ja recebeu, numero é ", message.number, " mas esperavamos ", remote_expected_message_number);
            return;
        }
        remote_expected_message_number++;

        switch (message.type)
        {
        case MessageType::DATA:
            // forward pra aplicação
            log_debug("recebeu msg da aplicacao: ", message.data);
            break;

        default:
            break;
        }
    }

    void change_state(ConnectionState new_state)
    {
        if (state == new_state)
            return;
        state = new_state;
        state_change.notify_all();
    }

    bool connect()
    {
        if (state == ConnectionState::ESTABLISHED)
            return true;

        change_state(ConnectionState::SYN_SENT);
        local_next_message_number = 0;
        Packet p = empty_packet();
        p.data.header.syn = 1; // Dá pra simplificar essa parte
        log_debug("connect: Sending SYN.");
        send(p);

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

        change_state(ConnectionState::FIN_WAIT);
        local_next_message_number = 0;
        remote_expected_message_number = 0;
        local_unacknowlodged_message_number = 0;
        Packet p = empty_packet();
        p.data.header.fin = 1;
        log_debug("disconnect: sending FIN.");
        send(p);

        std::unique_lock lock(mutex);
        while (state != ConnectionState::CLOSED)
            state_change.wait(lock); // TODO: timeout

        log_debug("disconnect: connection closed successfully.");
        return true;
    }

    Packet empty_packet()
    {
        PacketHeader header;
        memset(&header, 0, sizeof(PacketHeader));
        header.msg_num = 0;
        header.type = MessageType::CONTROL;

        PacketData data;
        memset(&data, 0, sizeof(PacketData));
        data.header = header;

        Packet packet;
        memset(&packet, 0, sizeof(Packet));
        packet.meta.origin = local_node.get_address();
        packet.meta.destination = remote_node.get_address();
        packet.data = data;

        // remote_expected_message_number++;
        // local_next_message_number++;

        return packet;
    }

    void closed(Packet p)
    {
        if (p.data.header.is_syn() && !p.data.header.is_ack())
        {
            log_debug("closed: received SYN; sending SYN+ACK.");
            // local_next_message_number = 1;
            change_state(ConnectionState::SYN_RECEIVED);
            Packet p = empty_packet();
            p.data.header.syn = 1;
            p.data.header.ack = 1;
            send(p);
        }

        /* pro reschedule: também tem q fazer alterar o número dos pacotes no transmissionlayer
        // atualmente, dá problema caso B tenha enviado 1 mensagem pra A, A cai, B tenta enviar a 2a
        // da pra resolver isso tanto por reschedule quanto por timeout
        if (p.data.header.msg_num != 0)
        {
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
                // remote_expected_message_number = p.data.header.get_message_number() + 1;
                log_debug("syn_sent: received SYN+ACK; sending ACK.");
                change_state(ConnectionState::ESTABLISHED);
                Packet p = empty_packet();
                p.data.header.ack = 1;
                remote_expected_message_number = 1;
                local_next_message_number = 1;
                local_unacknowlodged_message_number = 1;
                log_debug("Connection established; our current sequence number is ", local_next_message_number, ", and we expect to receive ", remote_expected_message_number, " from the remote.");
                send(p);
                return;
            }
            log_debug("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");
            Packet p = empty_packet();
            change_state(ConnectionState::SYN_RECEIVED);
            p.data.header.syn = 1;
            p.data.header.ack = 1;
            send(p);
            return;
        }

        if (p.data.header.is_rst())
        {
            // reiniciar conexao
            Packet p = empty_packet();
            p.data.header.syn = 1; // Dá pra simplificar essa parte
            send(p);
            return;
        }

        log_debug("syn_sent: received unexpected packet; closing connection.");
        change_state(ConnectionState::CLOSED);
    }

    void syn_received(Packet p)
    {
        if (p.data.header.is_rst())
        {
            log_debug("syn_received: received RST; closing.");
            change_state(ConnectionState::CLOSED);
            return;
        }

        if (p.data.header.is_ack())
        {
            remote_expected_message_number = 1;
            local_next_message_number = 1;
            local_unacknowlodged_message_number = 1;
            log_debug("syn_received: received ACK, connection established; our current sequence number is ", local_next_message_number, ", and we expect to receive ", remote_expected_message_number, " from the remote.");
            change_state(ConnectionState::ESTABLISHED);
            return;
        }

        log_debug("syn_received: received SYN; closing.");
        if (p.data.header.is_syn())
        {
            Packet p = empty_packet();
            p.data.header.rst = 1;
            change_state(ConnectionState::CLOSED);
            send(p);
        }
    }

    void established(Packet p);

    Packet create_ack_packet(Packet packet)
    {
        PacketData data;
        data.header = {// TODO: Definir corretamente checksum, window, e reserved.
                       msg_num : packet.data.header.msg_num,
                       fragment_num : packet.data.header.fragment_num,
                       checksum : 0,
                       window : 0,
                       type : MessageType::CONTROL,
                       ack : 1,
                       more_fragments : 0,
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
        return ack_packet;
    }

    void fin_wait(Packet p)
    {
        if (p.data.header.is_rst())
        {
            log_debug("last_ack: received RST; closing.");
            change_state(ConnectionState::CLOSED);
            return;
        }
        if (p.data.header.is_syn())
        {
            log_debug("last_ack: received SYN; closing and sending RST.");
            change_state(ConnectionState::CLOSED);
            Packet p = empty_packet();
            p.data.header.rst = 1;
            send(p);
            return;
        }

        if (p.data.header.is_fin())
        {
            if (p.data.header.is_ack())
            {
                log_debug("fin_wait: received FIN+ACK; closing and sending ACK.");
                change_state(ConnectionState::CLOSED);
                Packet p = empty_packet();
                p.data.header.ack = 1;
                send(p);
                return;
            }
            log_debug("fin_wait: simultaneous termination; transitioning to last_ack and sending ACK.");
            Packet p = empty_packet();
            p.data.header.ack = 1;
            send(p);
            change_state(ConnectionState::LAST_ACK);
        }
    }

    void last_ack(Packet p)
    {
        // graantir q o numero de seq é 0

        if (p.data.header.is_rst())
        {
            log_debug("last_ack: received RST; closing.");
            change_state(ConnectionState::CLOSED);
            return;
        }
        if (p.data.header.is_syn())
        {
            log_debug("last_ack: received SYN; closing and sending RST.");
            change_state(ConnectionState::CLOSED);
            Packet p = empty_packet();
            p.data.header.rst = 1;
            send(p);
            return;
        }

        if (p.data.header.is_ack())
        {
            log_debug("last_ack: received ACK; closing.");
            change_state(ConnectionState::CLOSED);
        }
    }

    uint32_t new_message_number()
    {
        return local_next_message_number++;
    }

    const uint32_t &get_expected_message_number()
    {
        return remote_expected_message_number;
    }
    void increment_expected_message_number()
    {
        remote_expected_message_number++;
    }
};

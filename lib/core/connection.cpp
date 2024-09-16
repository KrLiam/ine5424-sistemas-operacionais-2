#include "core/connection.h"
#include "communication/pipeline.h"

void Connection::send(Message message)
{
    if (!connect())
    {
        log_warn("Unable to establish a connection to ", remote_node.get_address().to_string(), ".");
        return;
    }
    message.number = new_message_number();
    pipeline.send(message);
}

void Connection::send(Packet packet)
{
    pipeline.send(packet);
}

void Connection::established(Packet p)
{
    // validar numero = 0
    if (p.data.header.is_rst())
    {
        log_debug("established: received RST; closing.");
        change_state(ConnectionState::CLOSED);
        return;
    }
    if (p.data.header.is_syn())
    {
        log_debug("established: received SYN; closing and sending RST.");
        change_state(ConnectionState::CLOSED);
        Packet p = empty_packet();
        p.data.header.rst = 1;
        send(p);
        return;
    }

    if (p.data.header.is_fin())
    {
        log_debug("established: received FIN; sending FIN+ACK.");
        local_next_message_number = 0;
        remote_expected_message_number = 0;
        local_unacknowlodged_message_number = 0;
        change_state(ConnectionState::LAST_ACK);
        Packet p = empty_packet();
        p.data.header.ack = 1;
        p.data.header.fin = 1;
        send(p);
        return;
    }

    // todo: enviar ack e seq
    bool is_ack = p.data.header.ack;
    if (is_ack)
    {
        log_debug("Received ACK for packet ", p.to_string(), "; removing from list of packets with pending ACKs.");
        pipeline.stop_transmission(p);
        return;
    }

    uint32_t message_number = p.data.header.msg_num;
    if (message_number > remote_expected_message_number)
    {
        log_debug("Received a packet ", p.to_string(), " that expects confirmation, but message number ", message_number, " is higher than the expected ", remote_expected_message_number, ".");
        return;
    }

    log_debug("Received a packet ", p.to_string(), " that expects confirmation; sending ACK.");
    Packet ack_packet = create_ack_packet(p);
    send(ack_packet);
}
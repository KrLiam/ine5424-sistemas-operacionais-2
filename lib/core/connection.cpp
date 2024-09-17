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
    if (close_on_rst(p))
        return;
    if (rst_on_syn(p))
        return;

    if (p.data.header.is_fin())
    {
        set_message_numbers(0);
        log_debug("established: received FIN; sending FIN+ACK.");
        change_state(ConnectionState::LAST_ACK);
        send_flag(ACK | FIN);
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
    if (message_number > expected_number)
    {
        log_debug("Received a packet ", p.to_string(), " that expects confirmation, but message number ", message_number, " is higher than the expected ", expected_number, ".");
        return;
    }

    log_debug("Received a packet ", p.to_string(), " that expects confirmation; sending ACK.");
    send_ack(p);
}
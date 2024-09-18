#include "communication/connection.h"
#include "pipeline/pipeline.h"

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

    if (p.data.header.is_ack())
    {
        log_debug("Received ACK for packet ", p.to_string(), "; removing from list of packets with pending ACKs.");
        pipeline.stop_transmission(p);
        return;
    }

    uint32_t message_number = p.data.header.get_message_number();
    if (message_number > expected_number)
    {
        log_debug("Received a packet ", p.to_string(), " that expects confirmation, but message number ", message_number, " is higher than the expected ", expected_number, "; ignoring it.");
        return;
    }

    if (p.data.header.get_message_type() == MessageType::APPLICATION && !application_buffer.can_produce())
    {
        log_warn("Application buffer is full; ignoring packet.");
        return;
    }

    log_debug("Received a packet ", p.to_string(), " that expects confirmation; sending ACK.");
    send_ack(p);

    if (p.data.header.get_message_type() == MessageType::APPLICATION && pipeline.is_message_complete(remote_node.get_id()))
    {
        log_debug("Message from ", remote_node.to_string(), " is complete; receiving it.");
        receive(pipeline.assemble_message(remote_node.get_id()));
    }
}

#include "communication/connection.h"
#include "pipeline/pipeline.h"

void Connection::connect() {
    if (state == ESTABLISHED)
        return;

    log_trace("connect: sending SYN.");
    reset_message_numbers();
    change_state(SYN_SENT);
    send_flag(SYN);
    connect_timer = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));

    // std::unique_lock lock(mutex);
    // while (state != ESTABLISHED && state != CLOSED)
    //     state_change.wait(lock);

    // if (state == CLOSED)
    // {
    //     log_warn("connect: unable to establish connection.");
    //     return false;
    // }
    // timer.cancel(timer_id);

    // log_trace("connect: connection established successfully.");
    // return true;
}

bool Connection::disconnect()
{
    if (state == CLOSED)
        return true;

    log_trace("disconnect: sending FIN.");
    change_state(FIN_WAIT);
    send_flag(FIN);
    int timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));

    std::unique_lock lock(mutex);
    while (state != CLOSED)
        state_change.wait(lock);
    timer.cancel(timer_id);

    log_trace("disconnect: connection closed.");
    return true;
}


void Connection::closed(Packet p)
{
    if (p.data.header.is_syn() && !p.data.header.is_ack())
    {
        expected_number++;
        log_trace("closed: received SYN; sending SYN+ACK.");
        change_state(SYN_RECEIVED);
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
        change_state(SYN_SENT);
        send(p);
        return;
    }*/
}

void Connection::syn_sent(Packet p)
{
    if (p.data.header.is_syn() && p.data.header.get_message_number() <= expected_number)
    {
        expected_number++;
        if (p.data.header.is_ack())
        {
            log_trace("syn_sent: received SYN+ACK; sending ACK.");
            log_info("syn_sent: connection established.");
            change_state(ESTABLISHED);
            send_flag(ACK);
            return;
        }
        log_trace("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");
        change_state(SYN_RECEIVED);
        send_flag(SYN | ACK);
        return;
    }

    if (p.data.header.is_rst())
    {
        expected_number++;
        log_trace("syn_sent: received RST; sending SYN.");
        send_flag(SYN);
        return;
    }

    log_trace("syn_sent: received unexpected packet; closing connection.");
    reset_message_numbers();
    change_state(CLOSED);
}

void Connection::syn_received(Packet p)
{
    if (close_on_rst(p))
        return;

    if (p.data.header.is_ack() && p.data.header.get_message_number() == expected_number)
    {
        expected_number++;
        log_trace("syn_received: received ACK.");
        log_info("syn_received: connection established.");
        change_state(ESTABLISHED);
        return;
    }

    if (rst_on_syn(p))
        return;
}

void Connection::established(Packet p)
{
    if (close_on_rst(p))
        return;
    if (rst_on_syn(p))
        return;

    if (p.data.header.is_fin())
    {
        log_trace("established: received FIN; sending FIN+ACK.");
        change_state(LAST_ACK);
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

    if (p.data.header.get_message_type() == MessageType::APPLICATION && pipeline.is_message_complete(p))
    {
        log_debug("Message from ", remote_node.to_string(), " is complete; receiving it.");
        receive(pipeline.assemble_message(p));
    }
}

void Connection::fin_wait(Packet p)
{
    if (close_on_rst(p))
        return;
    if (rst_on_syn(p))
        return;

    if (p.data.header.is_fin())
    {
        expected_number++;
        if (p.data.header.is_ack())
        {
            log_trace("fin_wait: received FIN+ACK; closing and sending ACK.");
            change_state(CLOSED);
            send_flag(ACK);
            reset_message_numbers();
            return;
        }
        log_trace("fin_wait: simultaneous termination; transitioning to last_ack and sending ACK.");
        change_state(LAST_ACK);
        send_flag(ACK);
    }
}

void Connection::last_ack(Packet p)
{
    if (close_on_rst(p))
        return;
    if (rst_on_syn(p))
        return;

    if (p.data.header.is_ack())
    {
        log_trace("last_ack: received ACK.");
        log_info("last_ack: connection closed.");
        reset_message_numbers();
        change_state(CLOSED);
    }
}


void Connection::send_flag(unsigned char flags)
{
    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));
    header.msg_num = next_number;
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

    next_number++;
    send(packet);
}

void Connection::send_ack(Packet packet)
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

bool Connection::close_on_rst(Packet p)
{
    if (p.data.header.is_rst())
    {
        log_debug(get_current_state_name(), ": received RST.");
        log_info(get_current_state_name(), ": connection closed.");
        reset_message_numbers();
        change_state(CLOSED);
        return true;
    }
    return false;
}

bool Connection::rst_on_syn(Packet p)
{
    if (p.data.header.is_syn())
    {
        log_debug(get_current_state_name(), ": received SYN; sending RST.");
        log_info(get_current_state_name(), ": connection closed.");
        next_number = 0;
        expected_number = p.data.header.get_message_number() + 1;
        change_state(CLOSED);
        send_flag(RST);
        return true;
    }
    return false;
}

uint32_t Connection::new_message_number()
{
    return next_number++;
}

void Connection::reset_message_numbers()
{
    expected_number = 0;
    next_number = 0;
}

std::string Connection::get_current_state_name()
{
    return state_names.at(state);
}

void Connection::change_state(ConnectionState new_state)
{
    if (state == new_state)
        return;
    state = new_state;
    state_change.notify_all();
}


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


void Connection::receive(Packet packet)
{
    packet_receive_handlers.at(state)(packet);
}
void Connection::receive(Message message)
{
    if (state != ESTABLISHED)
    {
        log_warn("Connection is not established; dropping message ", message.to_string(), ".");
        return;
    }

    if (message.number < expected_number)
    {
        log_warn("Message ", message.to_string(), " was already received; dropping it.");
        return;
    }

    if (message.number > expected_number)
    {
        log_warn("Message ", message.to_string(), " is unexpected, current number expected is ", expected_number, "; dropping it.");
        return;
    }

    expected_number++;
    application_buffer.produce(message);
}

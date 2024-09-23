#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "utils/uuid.h"

Connection::Connection(
    Node local_node,
    Node remote_node,
    Pipeline &pipeline,
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> &application_buffer,
    BufferSet<std::string> &connection_update_buffer) : pipeline(pipeline),
                                                        application_buffer(application_buffer),
                                                        local_node(local_node),
                                                        remote_node(remote_node),
                                                        connection_update_buffer(connection_update_buffer)
{
    observe_pipeline();
}

void Connection::observe_pipeline()
{
    obs_message_defragmentation_is_complete.on(std::bind(&Connection::message_defragmentation_is_complete, this, _1));
    obs_transmission_complete.on(std::bind(&Connection::transmission_complete, this, _1));
    obs_transmission_fail.on(std::bind(&Connection::transmission_fail, this, _1));
    pipeline.attach(obs_message_defragmentation_is_complete);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
}

void Connection::message_defragmentation_is_complete(const MessageDefragmentationIsComplete &event)
{
    Packet &packet = event.packet;
    if (packet.meta.origin != remote_node.get_address())
        return;

    if (application_buffer.can_produce())
        pipeline.notify(ForwardDefragmentedMessage(packet));
}

void Connection::transmission_complete(const TransmissionComplete &event)
{
    if (!active_transmission)
        return;

    const UUID &uuid = event.uuid;

    if (uuid != active_transmission->uuid)
        return;

    active_transmission->set_result(true);
    complete_transmission();
}

void Connection::transmission_fail(const TransmissionFail &event)
{
    if (!active_transmission)
        return;

    Packet &packet = event.faulty_packet;
    const UUID &uuid = packet.meta.transmission_uuid;

    if (uuid != active_transmission->uuid)
        return;

    active_transmission->set_result(false);
    complete_transmission();
}

void Connection::connect()
{
    if (state == ESTABLISHED)
        return;

    log_trace("connect: sending SYN.");
    reset_message_numbers();
    change_state(SYN_SENT);
    send_flag(SYN);
    handshake_timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
}

void Connection::connection_timeout()
{
    log_warn("Unable to establish connection with node ", remote_node.get_id());
    change_state(CLOSED);
    handshake_timer_id = -1;
    cancel_transmissions();
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
        next_number = 0;
        expected_number = p.data.header.get_message_number() + 1;
        log_trace("closed: received SYN; sending SYN+ACK.");
        change_state(SYN_RECEIVED);
        send_flag(SYN | ACK);
        handshake_timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
        return;
    }

    log_trace("closed: unexpected packet; sending RST.");
    reset_message_numbers();
    send_flag(RST);
}

void Connection::syn_sent(Packet p)
{
    if (close_on_rst(p))
        return;

    if (p.data.header.is_syn() && p.data.header.get_message_number() <= expected_number)
    {
        expected_number = p.data.header.get_message_number() + 1;
        if (p.data.header.is_ack())
        {
            log_trace("syn_sent: received SYN+ACK; sending ACK.");
            log_info("syn_sent: connection established.");

            timer.cancel(handshake_timer_id);
            handshake_timer_id = -1;

            change_state(ESTABLISHED);
            send_flag(ACK);
            return;
        }
        log_trace("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");
        change_state(SYN_RECEIVED);
        send_flag(SYN | ACK);
        handshake_timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
        return;
    }
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

        timer.cancel(handshake_timer_id);
        handshake_timer_id = -1;

        change_state(ESTABLISHED);
        return;
    }

    if (p.data.header.is_syn())
    {
        next_number = 0;
        expected_number++;
        send_flag(SYN | ACK);
    }
}

void Connection::established(Packet p)
{
    if (p.data.header.is_rst())
    {
        if (active_transmission)
        {
            active_transmission->active = false;
            pipeline.notify(PipelineCleanup(active_transmission->message));
            active_transmission = nullptr;
        }

        reset_message_numbers();
        expected_number++;
        change_state(SYN_SENT);
        send_flag(SYN);
        handshake_timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
        return;
    }

    if (p.data.header.is_syn())
    {
        reset_message_numbers();
        expected_number++;
        change_state(SYN_SENT);
        send_flag(SYN);
        handshake_timer_id = timer.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
    }

    if (p.data.header.is_fin())
    {
        log_trace("established: received FIN; sending FIN+ACK.");
        change_state(LAST_ACK);
        send_flag(ACK | FIN);
        return;
    }

    if (p.data.header.is_ack())
    {
        log_debug("Received ", p.to_string(PacketFormat::RECEIVED), "; removing from list of packets with pending ACKs.");
        pipeline.notify(PacketAckReceived(p));
        return;
    }

    uint32_t message_number = p.data.header.get_message_number();
    if (message_number > expected_number)
    {
        log_debug("Received ", p.to_string(PacketFormat::RECEIVED), " that expects confirmation, but message number ", message_number, " is higher than the expected ", expected_number, "; ignoring it.");
        return;
    }

    if (p.data.header.get_message_type() == MessageType::APPLICATION && !application_buffer.can_produce())
    {
        log_warn("Application buffer is full; ignoring packet.");
        return;
    }

    log_debug("Received ", p.to_string(PacketFormat::RECEIVED), " that expects confirmation; sending ACK.");
    send_ack(p);
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
    packet.meta.origin = local_node.get_address();
    packet.meta.destination = remote_node.get_address();
    packet.data = data;

    next_number++;
    transmit(packet);
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
                   end : 0,
                   type : MessageType::CONTROL,
                   reserved : 0
    };
    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        origin : local_node.get_address(),
        destination : remote_node.get_address(),
        time : 0,
        message_length : 0,
        expects_ack : 0
    };
    Packet ack_packet = {
        data : data,
        meta : meta
    };

    transmit(ack_packet);
}

bool Connection::close_on_rst(Packet p)
{
    if (p.data.header.is_rst() && p.data.header.get_message_number() == expected_number)
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

    if (state == ConnectionState::ESTABLISHED)
    {
        request_update();
    }
}

void Connection::enqueue(Transmission &transmission)
{
    mutex_transmissions.lock();
    transmissions.push_back(&transmission);
    mutex_transmissions.unlock();

    log_debug("Enqueued transmission ", transmission.uuid, " on connection of node ", transmission.receiver_id);

    request_update();
}

void Connection::request_update()
{
    connection_update_buffer.produce(remote_node.get_id());
}

void Connection::complete_transmission()
{
    if (!active_transmission)
        return;

    int size = transmissions.size();
    for (int i = 0; i < size; i++)
    {
        if (transmissions[i] != active_transmission)
            continue;

        transmissions.erase(transmissions.begin() + i);
        break;
    }

    active_transmission->release();
    active_transmission = nullptr;

    if (transmissions.size())
        request_update();
}

void Connection::cancel_transmissions()
{
    mutex_transmissions.lock();

    for (Transmission *transmission : transmissions)
    {
        transmission->set_result(false);
        transmission->release();
    }

    transmissions.clear();
    active_transmission = nullptr;
    mutex_transmissions.unlock();

    mutex_packets.lock();
    packets_to_send.clear();
    mutex_packets.unlock();
}

void Connection::update()
{
    log_trace("Updating connection with node ", remote_node.get_id());

    mutex_packets.lock();
    for (Packet p : packets_to_send)
        send(p);
    packets_to_send.clear();
    mutex_packets.unlock();

    if (!transmissions.size()) return;

    if (state == ConnectionState::CLOSED)
    {
        connect();
        return;
    }

    if (state != ConnectionState::ESTABLISHED)
        return;

    if (!active_transmission)
    {
        active_transmission = transmissions[0];
        active_transmission->active = true;
        send(active_transmission->message);
    }
}

void Connection::send(Message message)
{
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

void Connection::transmit(Packet p)
{
    mutex_packets.lock();
    packets_to_send.push_back(p);
    mutex_packets.unlock();
    request_update();
}

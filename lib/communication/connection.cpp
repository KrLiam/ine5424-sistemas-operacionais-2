#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "utils/uuid.h"

Connection::Connection(
    Node local_node,
    Node remote_node,
    Pipeline &pipeline,
    Buffer<Message> &application_buffer,
    BufferSet<std::string> &connection_update_buffer,
    const TransmissionDispatcher& broadcast_dispatcher
) :
    pipeline(pipeline),
    application_buffer(application_buffer),
    broadcast_dispatcher(broadcast_dispatcher),
    local_node(local_node),
    remote_node(remote_node),
    connection_update_buffer(connection_update_buffer),
    dispatcher(remote_node.get_id(), connection_update_buffer, pipeline)                                          
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
    if (packet.data.header.id.origin != remote_node.get_address())
        return;

    if (application_buffer.can_produce())
        pipeline.notify(ForwardDefragmentedMessage(packet));
}

void Connection::transmission_complete(const TransmissionComplete &event)
{
    const UUID &uuid = event.uuid;
    const Transmission* active = dispatcher.get_active();

    if (active && uuid == active->uuid) dispatcher.complete(true);
}

void Connection::transmission_fail(const TransmissionFail &event)
{
    const UUID &uuid = event.faulty_packet.meta.transmission_uuid;
    const Transmission* active = dispatcher.get_active();

    if (active && uuid == active->uuid) dispatcher.complete(false);
}

ConnectionState Connection::get_state() const { return state; }

void Connection::connect()
{
    if (state != CLOSED)
        return;

    log_trace("connect: sending SYN.");
    reset_message_numbers();
    change_state(SYN_SENT);
    send_syn(0);
    set_timeout();
}

void Connection::set_timeout()
{
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
    if (p.data.header.is_syn() && !p.data.header.is_ack() && p.data.header.get_message_number() == 0)
    {
        log_trace("closed: received SYN; sending SYN+ACK.");
        reset_message_numbers();
        resync_broadcast_on_syn(p);
        send_syn(ACK);
        change_state(SYN_RECEIVED);
        set_timeout();
        return;
    }

    log_trace("closed: unexpected packet; sending RST.");
    reset_message_numbers();
    send_flag(RST);
}

void Connection::on_established() {
    ConnectionEstablished event(remote_node);
    pipeline.notify(event);
}

void Connection::on_closed() {
    ConnectionClosed event(remote_node);
    pipeline.notify(event);
}

void Connection::syn_sent(Packet p)
{
    if (close_on_rst(p))
        return;

    if (p.data.header.get_message_number() != 0)
        return;

    if (p.data.header.is_syn())
    {
        if (p.data.header.is_ack())
        {
            log_trace("syn_sent: received SYN+ACK; sending ACK.");

            resync_broadcast_on_syn(p);

            send_flag(ACK);
            dispatcher.reset_number(1);
            expected_number = 1;

            log_info("syn_sent: connection established.");
            change_state(ESTABLISHED);

            return;
        }

        log_trace("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");
        
        resync_broadcast_on_syn(p);

        send_syn(ACK);
        change_state(SYN_RECEIVED);
        set_timeout();

        return;
    }
}

void Connection::syn_received(Packet p)
{
    if (close_on_rst(p))
        return;

    if (p.data.header.get_message_number() != 0)
        return;

    if (p.data.header.is_ack())
    {
        dispatcher.reset_number(1);
        expected_number = 1;
        log_trace("syn_received: received ACK.");

        change_state(ESTABLISHED);
        log_info("syn_received: connection established.");

        return;
    }

    if (p.data.header.is_syn())
    {

        if (p.data.header.is_ack())
        {
            log_debug("syn_received: received SYN+ACK.");

            resync_broadcast_on_syn(p);
            send_flag(ACK);
            dispatcher.increment_number();

            change_state(ESTABLISHED);
            log_info("syn_received: connection established.");
            
            return;
        }
        log_debug("syn_received: received SYN; sending SYN+ACK.");
        send_syn(ACK);
    }
}

void Connection::established(Packet p)
{
    if (p.data.header.is_rst())
    {
        cancel_transmissions();
        reset_message_numbers();
        send_syn(0);
        change_state(SYN_SENT);
        set_timeout();
        return;
    }

    if (p.data.header.is_syn() && p.data.header.get_message_number() == 0)
    {
        cancel_transmissions();
        reset_message_numbers();
        resync_broadcast_on_syn(p);
        send_syn(ACK);
        change_state(SYN_RECEIVED);
        set_timeout();
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
    MessageSequenceType type = p.data.header.id.sequence_type;

    uint32_t number = type == MessageSequenceType::BROADCAST ? expected_broadcast_number : expected_number;
    uint32_t initial_number = type == MessageSequenceType::BROADCAST ? initial_broadcast_number : 0;

    if (message_number < initial_number)
    {
        log_debug(
            "Received ",
            p.to_string(PacketFormat::RECEIVED),
            " which is prior to current connection with initial number ",
            initial_number,
            "; ignoring it."
        );
        return;
    }
    if (message_number > number)
    {
        log_debug(
            "Received ",
            p.to_string(PacketFormat::RECEIVED),
            " that expects confirmation, but message number ",
            message_number,
            " is higher than the expected ",
            number,
            "; ignoring it."
        );
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
        // expected_number++;
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

void Connection::send_syn(uint8_t extra_flags)
{
    SynData data = {
        broadcast_number : broadcast_dispatcher.get_next_number()
    };

    send_flag(SYN | extra_flags, MessageData::from(data));
}

void Connection::send_flag(uint8_t flags) {
    return send_flag(flags, {nullptr, 0});
}
void Connection::send_flag(uint8_t flags, MessageData message_data)
{
    MessageIdentity id = {
        origin : local_node.get_address(),
        msg_num : dispatcher.get_next_number(),
        sequence_type : MessageSequenceType::UNICAST
    };

    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));

    header.id = id;
    header.type = MessageType::CONTROL;
    header.flags = flags;

    PacketData data;
    memset(&data, 0, sizeof(PacketData));
    data.header = header;

    if (message_data.size)
    {
        memcpy(&data.message_data, message_data.ptr, message_data.size);
    }

    Packet packet;
    packet.meta.destination = remote_node.get_address();
    packet.meta.message_length = message_data.size;
    packet.data = data;

    transmit(packet);
}

void Connection::send_ack(Packet packet)
{
    PacketData data;
    memset(&data, 0, sizeof(PacketData));

    data.header = {
        id : {
            origin : local_node.get_address(),
            msg_num : packet.data.header.id.msg_num,
            sequence_type : packet.data.header.id.sequence_type
        },
        fragment_num : packet.data.header.fragment_num,
        checksum : 0,
        flags : ACK,
        type : MessageType::CONTROL
    };
    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        destination : remote_node.get_address(),
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
        reset_message_numbers();
        change_state(CLOSED);
        send_flag(RST);
        return true;
    }
    return false;
}

bool Connection::resync_broadcast_on_syn(Packet p) {
    if (!p.data.header.is_syn() || p.meta.message_length != sizeof(SynData)) return false;

    SynData* data = reinterpret_cast<SynData*>(p.data.message_data);
    
    initial_broadcast_number = data->broadcast_number;
    expected_broadcast_number = initial_broadcast_number;
    log_info("Broadcast sequence with node ", local_node.get_id(), " was resync to ", initial_broadcast_number);

    return true;
}

void Connection::reset_message_numbers()
{
    expected_number = 0;
    dispatcher.reset_number(0);
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

    if (handshake_timer_id != -1)
    {
        timer.cancel(handshake_timer_id);
        handshake_timer_id = -1;
    }

    if (state == ConnectionState::ESTABLISHED)
    {
        request_update();
    }

    if (post_transition_handlers.contains(state)) {
        post_transition_handlers.at(state)();
    }
}

bool Connection::enqueue(Transmission &transmission)
{
    return dispatcher.enqueue(transmission);
}

void Connection::request_update()
{
    connection_update_buffer.produce(remote_node.get_id());
}

void Connection::cancel_transmissions()
{
    dispatcher.cancel_all();

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

    if (dispatcher.is_empty()) return;

    if (state == ConnectionState::CLOSED)
    {
        connect();
        return;
    }

    if (state == ConnectionState::ESTABLISHED) dispatcher.update();
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

    MessageSequenceType type = message.id.sequence_type;
    uint32_t number = type == MessageSequenceType::BROADCAST ? expected_broadcast_number : expected_number;

    if (message.id.msg_num < number)
    {
        log_warn("Message ", message.to_string(), " was already received; dropping it.");
        return;
    }

    if (message.id.msg_num > number)
    {
        log_warn("Message ", message.to_string(), " is unexpected, current number expected is ", number, "; dropping it.");
        return;
    }

    if (type == MessageSequenceType::BROADCAST)
    {
        expected_broadcast_number++;
        application_buffer.produce(message); // TODO buffer separado pro deliver
    }
    else
    {
        expected_number++;
        application_buffer.produce(message);
    }

}

void Connection::transmit(Packet p)
{
    mutex_packets.lock();
    packets_to_send.push_back(p);
    mutex_packets.unlock();
    request_update();
}

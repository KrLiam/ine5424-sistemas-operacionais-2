#include "communication/connection.h"
#include "pipeline/pipeline.h"
#include "utils/uuid.h"

Connection::Connection(
    Node& local_node,
    Node& remote_node,
    Pipeline &pipeline,
    Buffer<Message> &application_buffer,
    BufferSet<std::string> &connection_update_buffer,
    const TransmissionDispatcher& broadcast_dispatcher,
    const TransmissionDispatcher& ab_dispatcher
) :
    pipeline(pipeline),
    application_buffer(application_buffer),
    broadcast_dispatcher(broadcast_dispatcher),
    ab_dispatcher(ab_dispatcher),
    local_node(local_node),
    remote_node(remote_node),
    connection_update_buffer(connection_update_buffer),
    dispatcher(remote_node.get_id(), connection_update_buffer, pipeline)
{
    observe_pipeline();
}

void Connection::observe_pipeline()
{
    obs_packet_received.on(std::bind(&Connection::packet_received, this, _1));
    obs_message_received.on(std::bind(&Connection::message_received, this, _1));
    obs_message_defragmentation_is_complete.on(std::bind(&Connection::message_defragmentation_is_complete, this, _1));
    obs_transmission_complete.on(std::bind(&Connection::transmission_complete, this, _1));
    obs_transmission_fail.on(std::bind(&Connection::transmission_fail, this, _1));
    obs_node_death.on(std::bind(&Connection::node_death, this, _1));
    pipeline.attach(obs_packet_received);
    pipeline.attach(obs_message_received);
    pipeline.attach(obs_message_defragmentation_is_complete);
    pipeline.attach(obs_transmission_complete);
    pipeline.attach(obs_transmission_fail);
    pipeline.attach(obs_node_death); // TODO: arrumar o local disso, já q uma conexão fechar não faz parte do pipeline
}

void Connection::packet_received(const PacketReceived &event)
{
    if (event.packet.meta.origin != remote_node.get_address()) return;
    if (event.packet.data.header.get_message_type() == MessageType::RAFT) return;
    if (message_type::is_broadcast(event.packet.data.header.get_message_type())) return;

    receive(event.packet);
}

void Connection::message_received(const MessageReceived &event)
{
    if (event.message.origin != remote_node.get_address()) return;
    if (message_type::is_broadcast(event.message.id.msg_type)) return;
    receive(event.message);
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
    const MessageType& type = event.id.msg_type;
    bool is_our_atomic = message_type::is_atomic(type) && event.source == local_node.get_address();

    if (message_type::is_broadcast(type) && !is_our_atomic) return;
    if (type == MessageType::AB_REQUEST) return;

    const Transmission* active = dispatcher.get_active();
    if (!active) return;

    const UUID &uuid = event.uuid;

    if (uuid == active->uuid || (is_our_atomic && active->message.id.msg_type == MessageType::AB_REQUEST))
        dispatcher.complete(true);
}

void Connection::transmission_fail(const TransmissionFail &event)
{
    const Transmission* active = dispatcher.get_active();

    if (active && event.uuid == active->uuid) dispatcher.complete(false);
}

void Connection::node_death(const NodeDeath& event)
{
    if (event.remote_node != remote_node)
        return;

    cancel_transmissions();
    change_state(CLOSED);
};

ConnectionState Connection::get_state() const { return state; }

void Connection::connect()
{
    if (state != CLOSED)
        return;

    log_debug("connect: sending SYN.");
    reset_message_numbers();
    change_state(SYN_SENT);
    send_syn(0);
    set_timeout();
}

void Connection::close()
{
    if (state == CLOSED) return;

    cancel_transmissions();
    change_state(CLOSED);
}

void Connection::set_timeout()
{
    mutex_timer.lock();
    cancel_timeout();
    handshake_timer_id = TIMER.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));
    mutex_timer.unlock();
}

void Connection::cancel_timeout()
{
    if (handshake_timer_id != -1)
    {
        TIMER.cancel(handshake_timer_id);
        handshake_timer_id = -1;
    }
}

void Connection::connection_timeout()
{
    log_warn("Unable to establish connection with node ", remote_node.get_id());
    change_state(CLOSED);

    mutex_timer.lock();
    handshake_timer_id = -1;
    mutex_timer.unlock();

    cancel_transmissions();
}

bool Connection::disconnect()
{
    if (state == CLOSED)
        return true;

    log_trace("disconnect: sending FIN.");
    change_state(FIN_WAIT);
    send_flag(FIN);
    int timer_id = TIMER.add(HANDSHAKE_TIMEOUT, std::bind(&Connection::connection_timeout, this));

    std::unique_lock lock(mutex);
    while (state != CLOSED)
        state_change.wait(lock);
    TIMER.cancel(timer_id);

    log_trace("disconnect: connection closed.");
    return true;
}

void Connection::closed(Packet p)
{
    if (p.data.header.is_syn() && !p.data.header.is_ack() && p.data.header.get_message_number() == 0)
    {
        log_debug("closed: received SYN; sending SYN+ACK.");
        reset_message_numbers();
        resync_broadcast_on_syn(p);
        send_syn(ACK);
        change_state(SYN_RECEIVED);
        set_timeout();
        return;
    }

    if (p.data.header.is_rst()) return;

    log_debug("closed: unexpected packet; sending RST.");
    reset_message_numbers();
    send_flag(RST);
}

void Connection::on_established() {
    log_info("Established connection with node ", remote_node.get_id(), " (", remote_node.get_address().to_string(), ").");

    ConnectionEstablished event(remote_node);
    pipeline.notify(event);
}

void Connection::on_closed() {
    log_info("Closed connection with node ", remote_node.get_id(), " (", remote_node.get_address().to_string(), ").");

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
            log_debug("syn_sent: received SYN+ACK; sending ACK.");
            stop_syn_transmission();

            resync_broadcast_on_syn(p);

            send_flag(ACK);
            dispatcher.reset_number(1);
            expected_number = 1;

            change_state(ESTABLISHED);

            return;
        }

        log_debug("syn_sent: received SYN from simultaneous connection; transitioning to syn_received and sending SYN+ACK.");

        stop_syn_transmission();
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
        stop_syn_transmission();

        dispatcher.reset_number(1);
        expected_number = 1;
        log_debug("syn_received: received ACK.");

        change_state(ESTABLISHED);

        return;
    }

    if (p.data.header.is_syn())
    {

        if (p.data.header.is_ack())
        {
            log_debug("syn_received: received SYN+ACK.");
            stop_syn_transmission();

            resync_broadcast_on_syn(p);
            send_flag(ACK);
            dispatcher.increment_number();

            change_state(ESTABLISHED);
            
            return;
        }
        log_debug("syn_received: received SYN; sending SYN+ACK.");
        stop_syn_transmission();
        send_syn(ACK);
    }
}

void Connection::established(Packet p)
{
    if (p.data.header.is_rst())
    {
        cancel_transmissions();
        // reset_message_numbers();
        // send_syn(0);
        change_state(CLOSED);
        // set_timeout();
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
        return;
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

    uint32_t number = expected_number;
    uint32_t initial_number = 0;

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
            "; jumping sequence number."
        );
        expected_number = message_number;
    }

    if (message_type::is_application(p.data.header.get_message_type()) && !application_buffer.can_produce())
    {
        log_warn("Application buffer is full; ignoring packet.");
        return;
    }

    log_debug("Received ", p.to_string(PacketFormat::RECEIVED), " that expects confirmation.");
    dispatch_to_sender(create_ack(p));
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
        reset_message_numbers();
        change_state(CLOSED);
    }
}

void Connection::send_syn(uint8_t extra_flags)
{
    SynData data = {
        broadcast_number : broadcast_dispatcher.get_next_number(),
        ab_number : ab_dispatcher.get_next_number() + local_node.is_receiving_ab_broadcast()
    };

    send_flag(SYN | extra_flags, MessageData::from(data));
}

void Connection::send_flag(uint8_t flags) {
    return send_flag(flags, {nullptr, 0});
}
void Connection::send_flag(uint8_t flags, MessageData message_data)
{
    MessageIdentity id;
    memset(&id, 0, sizeof(MessageIdentity));

    id.origin = local_node.get_address(),
    id.msg_num = dispatcher.get_next_number();
    id.msg_type = MessageType::CONTROL;

    PacketHeader header;
    memset(&header, 0, sizeof(PacketHeader));

    header.id = id;
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
    packet.meta.expects_ack = flags & SYN;
    packet.data = data;

    dispatch_to_sender(packet);
}

bool Connection::close_on_rst(Packet p)
{
    if (p.data.header.is_rst() && p.data.header.get_message_number() == expected_number)
    {
        log_debug(get_current_state_name(), ": received RST.");
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
    
    pipeline.notify(ReceiveSynchronization(remote_node, expected_number, data->broadcast_number, data->ab_number));

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

    mutex_timer.lock();
    cancel_timeout();
    mutex_timer.unlock();

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
    try {
        connection_update_buffer.produce(remote_node.get_id());
    }
    catch (std::runtime_error& e) {}
}

void Connection::cancel_transmissions()
{
    dispatcher.cancel_all();

    mutex_dispatched_packets.lock();
    dispatched_packets.clear();
    mutex_dispatched_packets.unlock();
}

void Connection::update()
{
    // log_trace("Updating connection with node ", remote_node.get_id());

    send_dispatched_packets();

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
    if (packet.data.header.get_message_type() == MessageType::HEARTBEAT) return;

    packet_receive_handlers.at(state)(packet);
}
void Connection::receive(Message message)
{
    if (state != ESTABLISHED)
    {
        log_warn("Connection is not established; dropping message ", message.to_string(), ".");
        return;
    }

    uint32_t number = expected_number;

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

    expected_number++;
    
    pipeline.notify(UnicastMessageReceived(message));

    if (message.is_application())
    {
        application_buffer.produce(message);
    }
}

void Connection::dispatch_to_sender(Packet p)
{
    mutex_dispatched_packets.lock();
    dispatched_packets.push_back(p);
    mutex_dispatched_packets.unlock();

    request_update();
}

void Connection::heartbeat(std::unordered_set<SocketAddress> &suspicions)
{
    // log_trace("Heartbeating to ", remote_node.get_address().to_string(), ".");

    HeartbeatData hb_data = {
        suspicions: {0}
    };

    int total_size = 0;
    for (SocketAddress suspicion : suspicions) {
        if (total_size + SocketAddress::SERIALIZED_SIZE > PacketData::MAX_MESSAGE_SIZE) break;
        memcpy(&hb_data.suspicions[total_size], suspicion.serialize().c_str(), SocketAddress::SERIALIZED_SIZE);
        total_size += SocketAddress::SERIALIZED_SIZE;
    }

    uint8_t flags = local_node.is_leader() ? LDR : 0;

    PacketData data;
    memset(&data, 0, sizeof(PacketData));
    memcpy(data.message_data, &hb_data, sizeof(hb_data));
    data.header = {
        id : {
            origin : local_node.get_address(),
            msg_num : 0,
            msg_type : MessageType::HEARTBEAT
        },
        fragment_num: 0,
        checksum: 0,
        flags: flags,
        pid: {0}
    };

    PacketMetadata meta = {
        transmission_uuid : UUID(""),
        origin : local_node.get_address(),
        destination : remote_node.get_address(),
        message_length : total_size,
        expects_ack : 0,
        silent : 1
    };

    Packet heartbeat_packet = {
        data : data,
        meta : meta
    };

    dispatch_to_sender(heartbeat_packet);
}

void Connection::send_dispatched_packets()
{
    mutex_dispatched_packets.lock();

    for (Packet p : dispatched_packets) send(p);
    dispatched_packets.clear();

    mutex_dispatched_packets.unlock();
}

void Connection::stop_syn_transmission()
{
    log_debug("Stopping SYN transmission to ", remote_node.get_address().to_string(), ".");
    MessageIdentity id;
    memset(&id, 0, sizeof(MessageIdentity));

    id.origin = local_node.get_address(),
    id.msg_num = dispatcher.get_next_number();
    id.msg_type = MessageType::CONTROL;

    pipeline.notify(PipelineCleanup(id, remote_node.get_address()));
}
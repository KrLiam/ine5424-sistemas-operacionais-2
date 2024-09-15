#include "communication/transmission_layer.h"

static void run_receiver_thread(TransmissionLayer *manager)
{
    log_info("Initialized transmission layer receiver thread.");
    while (!manager->stop_threads)
    {
        manager->receiver_thread();
    }
    log_info("Closing transmission layer receiver thread.");
}

static void run_sender_thread(TransmissionLayer *manager)
{
    log_info("Initialized transmission layer sender thread.");
    while (!manager->stop_threads)
    {
        manager->sender_thread();
    }
    log_info("Closing transmission layer sender thread.");
}

TransmissionLayer::TransmissionLayer(PipelineHandler handler, GroupRegistry *gr, Channel *channel) : PipelineStep(handler, gr), channel(channel), stop_threads(false)
{
    service();
}

TransmissionLayer::~TransmissionLayer()
{
    stop_threads = true;
    channel->shutdown_socket();

    if (listener_thread_obj.joinable())
        listener_thread_obj.join();
    if (sender_thread_obj.joinable())
        sender_thread_obj.join();

    for (auto [id, queue] : queue_map)
        delete queue;
}

void TransmissionLayer::service()
{
    listener_thread_obj = std::thread(run_receiver_thread, this);
    sender_thread_obj = std::thread(run_sender_thread, this);
}

void TransmissionLayer::receiver_thread()
{
    Packet packet = channel->receive();
    if (packet != Packet{})
    {
        receive(packet);
    }
}

void TransmissionLayer::sender_thread()
{
    // TODO: essa thread tá consumindo 100% de cpu. tem q fazer algum semáforo pra travar ela, aí libera no send() e timeout de msg
    // adicionar process_step() ao inicio do pipeline de send

    // anotacao: o process step poderia, no send, ver se já existe conexao, e se nao, encaminhar msgs de handshake
    // pra etapa de fragmentacao antes de encaminhar a msg q o usuario quer enviar

    // TODO: atualmente, não tem controle de enviar 1 msg só por vez
    for (auto &[id, queue] : queue_map)
    {
        queue->send_timedout_packets(channel);
        // if (queue.has_sent_everything()) queue_map.erase(id); <- mutex pra esse mapa
    }
}

void TransmissionLayer::send(Packet packet)
{
    log_debug("Packet [", packet.to_string(), "] sent to transmission layer.");
    // tinha pensado em fazer o send daqui aguardar sincronamente, mas aí ele poderia travar a receiver_thread
    // TODO:
    Node destination = gr->get_node(packet.meta.destination);
    if (!queue_map.contains(destination.get_id()))
    {
        queue_map.emplace(destination.get_id(), new TransmissionQueue());
    }
    queue_map.at(destination.get_id())->add_packet_to_queue(packet);
    // esse aqui cospe no buffer da sender_thread
}

void TransmissionLayer::receive(Packet packet)
{
    log_debug("Packet [", packet.to_string(), "] received on transmission layer.");

    if (!gr->packet_originates_from_group(packet))
    {
        log_debug("Ignoring packet ", packet.to_string(), ", as it did not originate from the group.");
        return;
    }

    if (process_ack_field_of_received_packet(packet))
        return;

    Node origin = gr->get_node(packet.meta.origin);
    Connection connection = gr->get_connection(origin.get_id());

    // TODO: Ver se pode ser int mesmo
    uint32_t message_number = packet.data.header.msg_num;
    if (message_number != connection.get_expected_message_number())
    {
        log_debug("Received message with number ", message_number, ", but expected ", connection.get_expected_message_number(), ".");
        return;
    }

    uint8_t type = (uint8_t)packet.data.header.type;
    if (type == MessageType::DATA && !handler.can_forward_to_application())
    {
        log_debug("Incoming buffer is full; ignoring packet ", packet.to_string(), ".");
        return;
    }

    handler.forward_receive(packet);
}

bool TransmissionLayer::process_ack_field_of_received_packet(Packet packet)
{
    Node origin = gr->get_node(packet.meta.origin);
    Connection connection = gr->get_connection(origin.get_id()); // melhorar pra n precisar pegar o ndo toda vez q quer pegar a conexao

    bool is_ack = packet.data.header.ack;
    if (is_ack)
    {
        log_debug("Received ACK for packet ", packet.to_string(), "; removing from list of packets with pending ACKs.");
        if (queue_map.contains(origin.get_id()))
        {
            queue_map.at(origin.get_id())->mark_packet_as_acked(packet);
        }
        return true;
    }

    uint32_t message_number = packet.data.header.msg_num;
    if (message_number > connection.get_expected_message_number())
    {
        log_debug("Received a packet ", packet.to_string(), " that expects confirmation, but message number ", message_number ," is higher than the expected ", connection.get_expected_message_number(), ".");
        return false;
    }

    log_debug("Received a packet ", packet.to_string(), " that expects confirmation; sending ACK.");
    Packet ack_packet = create_ack_packet(packet);
    send(ack_packet);
    return false;
}

Packet TransmissionLayer::create_ack_packet(Packet packet)
{
    PacketData data;
    data.header = {// TODO: Definir corretamente checksum, window, e reserved.
                   msg_num : packet.data.header.msg_num,
                   fragment_num : packet.data.header.fragment_num,
                   checksum : 0,
                   window : 0,
                   type : packet.data.header.type,
                   ack : 1,
                   more_fragments : 0,
                   reserved : 0
    };
    PacketMetadata meta = {
        origin : gr->get_local_node().get_address(),
        destination : gr->get_node(packet.meta.origin).get_address(),
        time : 0,
        message_length : 0
    };
    Packet ack_packet = {
        data : data,
        meta : meta
    };
    return ack_packet;
}

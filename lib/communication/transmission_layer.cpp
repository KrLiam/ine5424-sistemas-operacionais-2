#include "communication/transmission_layer.h"

static void run_receiver_thread(TransmissionLayer *manager)
{
    log_info("Initialized transmission layer receiver thread.");
    while (true)
    {
        manager->receiver_thread();
    }
    log_info("Closing transmission layer receiver thread.");
}

static void run_sender_thread(TransmissionLayer *manager)
{
    log_info("Initialized transmission layer sender thread.");
    while (true)
    {
        manager->sender_thread();
    }
    log_info("Closing transmission layer sender thread.");
}

TransmissionLayer::TransmissionLayer(PipelineHandler handler, GroupRegistry &gr, Channel *channel) : PipelineStep(handler, gr), channel(channel)
{
    service();
}

TransmissionLayer::~TransmissionLayer()
{
}

void TransmissionLayer::service()
{
    std::thread listener_thread_obj(run_receiver_thread, this);
    listener_thread_obj.detach();
    std::thread sender_thread_obj(run_sender_thread, this);
    sender_thread_obj.detach();
}

void TransmissionLayer::receiver_thread()
{
    Packet packet = channel->receive();
    char pkt[sizeof(Packet)];
    memcpy(pkt, &packet, sizeof(Packet));
    receive(pkt);
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

void TransmissionLayer::send(char *m)
{
    Packet packet;
    memcpy(&packet, m, sizeof(Packet));
    log_debug("Packet [", packet.to_string(), "] sent to transmission layer.");
    // tinha pensado em fazer o send daqui aguardar sincronamente, mas aí ele poderia travar a receiver_thread
    // TODO:
    Node destination = gr.get_node(packet.meta.destination);
    if (!queue_map.contains(destination.get_id()))
    {
        queue_map.emplace(destination.get_id(), new TransmissionQueue());
    }
    queue_map.at(destination.get_id())->add_packet_to_queue(packet);
    // esse aqui cospe no buffer da sender_thread
}

void TransmissionLayer::receive(char *m)
{
    Packet packet;
    memcpy(&packet, m, sizeof(Packet));
    log_debug("Packet [", packet.to_string(), "] received on transmission layer.");

    Node origin = gr.get_node(packet.meta.origin);
    Connection *connection = gr.get_connection(origin.get_id());

    // TODO: Ver se pode ser int mesmo
    int message_number = packet.data.header.msg_num;
    if (message_number != connection->get_current_message_number())
    {
        log_debug("Received message with number ", message_number, ", but expected ", connection->get_current_message_number(), ".");
        return;
    }

    if (process_ack_of_received_packet(packet))
        return;
    handler.forward_receive(m);
}

bool TransmissionLayer::process_ack_of_received_packet(Packet packet)
{
    Node origin = gr.get_node(packet.meta.origin);

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

    log_debug("Received a packet ", packet.to_string(), " that expects confirmation; sending ACK.");
    PacketData data;
    data.header = { // TODO: Definir corretamente checksum, window, e reserved.
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
        origin : gr.get_local_node().get_address(),
        destination : gr.get_node(packet.meta.origin).get_address(),
        time : 0,
        message_length : 0
    };
    Packet ack_packet = {
        data : data,
        meta : meta
    };
    channel->send(ack_packet);
    return false;
}
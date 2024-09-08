#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <semaphore>
#include <thread>

#include "channels/channel.h"
#include "core/buffer.h"
#include "core/constants.h"
#include "core/segment.h"
#include "core/node.h"
#include "utils/format.h"
#include "utils/hash.h"

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);

    void receiver_thread()
    {
        // separar isso em camada de transmissão(recebimento), (des)fragmentação e processamento
        // receive_step(); - transmission_step
        Packet packet = channel->receive();
        
        Node origin = get_node(packet.meta.origin);
        Connection* connection = connections[origin.get_id()];
        if ((int)packet.data.header.msg_num != connection->get_msg_num())
        {
            log_debug("Received msg_num ", (int)packet.data.header.msg_num, ", but expected ", connection->get_msg_num(), ".");
            return;
        }

        bool is_ack = packet.data.header.ack;
        if (is_ack)
        {
            // remover o fragnum da lista pendente
            // tem q confirmar se tá nela msm tb
            log_debug("Received ACK for ", (int)packet.data.header.msg_num, "/", (int)packet.data.header.fragment_num, ".");
            connection->packets_awaiting_ack.erase(std::remove(connection->packets_awaiting_ack.begin(), connection->packets_awaiting_ack.end(), (int)packet.data.header.fragment_num), connection->packets_awaiting_ack.end());
            for (int i = 0; i < connection->packets_to_send.size(); i++)
            {
                Packet p = connection->packets_to_send[i];
                if (p.data.header.fragment_num == packet.data.header.fragment_num)
                {
                    connection->packets_to_send.erase(connection->packets_to_send.begin() + i);
                    break;
                }
            }
            return;
        } else {
            PacketData data;
            data.header = {
                msg_num: packet.data.header.msg_num,
                fragment_num: packet.data.header.fragment_num,
                type: packet.data.header.type,
                ack: 1,
                more_fragments: 0
                };
            PacketMetadata meta = {
                origin: get_local_node().get_address(),
                destination: origin.get_address(),
                time: 0,
                message_length: 0
            };
            Packet ack_packet = {
                data: data,
                meta: meta
            };
            log_debug("Sending ack to packet ", (int)packet.data.header.msg_num, "/", (int)packet.data.header.fragment_num, ".");
            channel->send(ack_packet); // isso tem q passar pela camada de checksum, criptografia
        }

        
        // fragments.produce(packet); <- buffer intermediario entre os niveis do pipeline
        
        //////////// defragmentation_step(); - fragmentation_step
        // if (!fragments.can_consume())
        //{
        //    return;
        //}
        //Packet packet = fragments.consume();

        //Connection connection = get_node(packet.meta.origin.to_string()).get_connection();
        /*if (connection->received_fragments.contains(packet.data.header.fragment_num))
        {
            return;
        }*/
        for (int frag_num : connection->received_fragments)
        {
            if (frag_num == packet.data.header.fragment_num)
            {
                log_debug("Already received packet ", frag_num);
                return;
            }
        }

        int frag_num = packet.data.header.fragment_num;
        bool more_fragments = packet.data.header.more_fragments;
        if (!more_fragments)
        {
            connection->last_fragment_num = packet.data.header.fragment_num;
        }

        strncpy(&connection->receive_buffer[packet.data.header.fragment_num * PacketData::MAX_MESSAGE_SIZE], packet.data.message_data, packet.meta.message_length);
        connection->received_fragments.push_back(packet.data.header.fragment_num);
        connection->bytes_received += packet.meta.message_length;

        if (connection->last_fragment_num == connection->received_fragments.size() - 1)
        {
            log_debug("Received all fragments; forwarding to message process step.");
            Message message = {
                origin: packet.meta.origin,
                destination: packet.meta.destination,
                length: connection->bytes_received
            };
            strncpy(message.data, connection->receive_buffer, connection->bytes_received);
            messages.produce(message);
            connection->received_fragments.clear();
            connection->bytes_received = 0;
        }

        // message_process_step();
        if (!messages.can_consume())
        {
            return;
        }
        Message message = messages.consume();
        
        switch (message.type)
        {
            case MessageType::DATA:
            receive_buffer.produce(message);
            break;

            default:
            break;
        }
    }

    void sender_thread()
    {
        // TODO: essa thread tá consumindo 100% de cpu. tem q fazer algum semáforo pra travar ela, aí libera no send() e timeout de msg
        
        process_step();
        // <fragmentacao>
        // CONSUMIR MSG DO BUFFER E FRAGMENTAR EM PACOTES
        fragmentation_step();

        // <controle de envio - transmission step>
        // ENVIAR PACKETS enquanto nao recebe ACKs
        transmission_step();
    }

    void process_step()
    {
        // dar peek no send_buffer e ver se já tá enviando algo pra aquele destino. se nao tiver, manda pra etapa de fragmentacao
        // aqui tem q incrementar o msg_num tb
    }

    void fragmentation_step()
    {
        if (!send_buffer.can_consume()) // trocar por um buffer de fragmentação
        {
            return;
        }
        Message message = send_buffer.consume();
        log_debug("Fragmentation step consumed message from send buffer.");

        Node destination = get_node(message.destination);
        Connection* connection = connections[destination.get_id()];

        int required_packets = ceil((double) message.length / PacketData::MAX_MESSAGE_SIZE);
        log_debug("Message length is ", message.length, "; will split into ", required_packets, " packets.");
        for (int i = 0; i < required_packets; i++)
        {
            PacketMetadata meta = {
                origin: get_local_node().get_address(),
                destination: destination.get_address(),
                time: 0
            };
            PacketData data;
            bool more_fragments = i != required_packets - 1;
            data.header = {
                msg_num: connection->get_msg_num(), // inicio 0
                fragment_num: i,
                type: message.type,
                ack: 0,
                more_fragments: more_fragments
                };
            meta.message_length = std::min((int)(message.length - i*PacketData::MAX_MESSAGE_SIZE), (int)PacketData::MAX_MESSAGE_SIZE);
            strncpy(data.message_data, &message.data[i*PacketData::MAX_MESSAGE_SIZE], meta.message_length);
            Packet packet = {
                data: data,
                meta: meta
            };

            log_debug("Forwarding packet ", (int)packet.data.header.msg_num, "/", (int)packet.data.header.fragment_num, " to transmission step.");
            connection->forward_packet(packet);
        }
    }

    unsigned int now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void transmission_step()
    {
        for (auto [id, connection] : connections) // talvez seria melhor ter só um mapa com as conexoes ativas
        {
            for (Packet &packet : connection->packets_to_send)
            {
                if (now() - packet.meta.time < ACK_TIMEOUT)
                {
                    continue;
                }
                channel->send(packet);
                packet.meta.time = now();
            }
        }
    }

    void send(std::string id, char *m)
    {
        Message message = {
            origin: get_local_node().get_address(),
            destination: get_node(id).get_address(), 
            length: user_buffer_size
            };
        strncpy(message.data, m, user_buffer_size);
        send_buffer.produce(message);
    }

    Message receive(char *m)
    {
        Message message = receive_buffer.consume();

        if (user_buffer_size >= message.length)
        {
            strncpy(m, message.data, message.length);
            return message;
        }

        throw std::runtime_error( "Provided buffer is smaller than the message.");
        /*log_warn("Buffer size defined by the user [", user_buffer_size , "] is smaller ",
        "than the received message's size [", message.length, "]; we will split this message into multiple ones.");
        int required_parts = ceil((double) message.length / user_buffer_size);
        for (int i = 0; i < required_parts; i++)
        {
            Message part = {
                origin: message.origin,
                destination: message.destination,
                type: message.type,
                part: i,
                has_more_parts: i != required_parts - 1
            };
            part.length = i * user_buffer_size;
            strncpy(part.data, &message.data[i * user_buffer_size], part.length);
            receive_buffer.produce(part);
        }

        return receive(m); // problema: perde a ordem */
    }

    const Node &get_node(std::string id)
    {
        std::map<std::string, Node>::iterator iterator = nodes.find(id);
        if (iterator != nodes.end())
        {
            return iterator->second;
        }
        throw std::invalid_argument(format("Node %s not found.", id.c_str()));
    }
    const Node &get_node(SocketAddress address)
    {
        for (auto &[id, node] : nodes)
        {
            if (address == node.get_address())
            {
                return node;
            }
        }
        throw std::invalid_argument(format("Node with address not found.", address.to_string()));
    }
    const std::map<std::string, Node> &get_nodes()
    {
        return nodes;
    }
    const Node &get_local_node()
    {
         return get_node(local_id);
    }

private:
    std::unique_ptr<Channel> channel;

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> receive_buffer{"message receive"};
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> send_buffer{"message send"};

    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> messages{"message processor"};

    std::size_t user_buffer_size;

    std::string local_id;
    std::map<std::string, Node> nodes;

    std::map<std::string, Connection*> connections;

    std::map<std::string, Node> create_nodes()
    {
        std::map<std::string, Node> _nodes;

        Config config = ConfigReader::parse_file("nodes.conf");
        for (NodeConfig node_config : config.nodes)
        {
            bool is_remote = local_id != node_config.id;
            Node node(node_config.id, node_config.address, is_remote);
            _nodes.emplace(node.get_id(), node);
        }

        return _nodes;
    }

    void create_connections()
    {
        // imeplementar handshake e fazer as conexões serem dinamicas
        for (auto& [id, node] : nodes)
        {
            Connection* c = new Connection();
            connections.emplace(id, c);
        }
    }
};

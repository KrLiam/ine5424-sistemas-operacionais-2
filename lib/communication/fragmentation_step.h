#include "communication/pipeline_step.h"

#include <map>

class FragmentationStep : public PipelineStep
{
public:
    FragmentationStep(StepControl control);

    void send(Message& m)
    {
        // TODO: lógica de janela
        int required_segments = m.size / Packet::DATA_SIZE;
        Segment segments[required_segments];

        for (int i = 0; i < required_segments; i++)
        {
            segments[i] = {origin: m.origin, destination: m.destination};
            segments[i].packet.header = {
               seq_num : 0, //
               fragment_num: i,
               type: MessageType.Data,
    int ack: 1;
    int more_fragments : 1;
    int reserved : 11;
            }
            strncpy(segments[i].packet.data, &m.data[i*Packet::DATA_SIZE], Packet::DATA_SIZE);
        }
    }

    void receive(Message& m)
    {
        log_warn("Fragmentation step's receive was called passing a message.");   
    }

    void send(Segment &segment)
    {
        log_debug("Forwarding segment.");
        control.forward_send(segment);
    }

    void receive(Segment &segment)
    {
        std::string key = segment.origin.to_string();

        if (!node_to_receive_buffer.contains(key))
        {
            node_to_receive_buffer.emplace(key, std::array<char, MAXIMUM_MESSAGE_SIZE>{});
        }
        if (!node_to_received_segments.contains(key))
        {
            node_to_received_segments.emplace(key, std::vector<int>{});
        }

        std::array<char, MAXIMUM_MESSAGE_SIZE> buffer = node_to_receive_buffer.at(key);
        std::vector<int> segments = node_to_received_segments.at(key);

        if (std::find(segments.begin(), segments.end(), (int)segment.packet.header.fragment_num) != segments.end())
        {
            log_debug("Already received fragment ", (int)segment.packet.header.fragment_num, " from ", key, ".");
            return;
        }

        strncpy(&buffer[segment.packet.header.seq_num], segment.packet.data, segment.data_size);
        segments.push_back(segment.packet.header.seq_num);

        if (segment.packet.header.more_fragments == 0)
        {
            log_debug("Last fragment number of message sent by ", key, " is ", (int)segment.packet.header.fragment_num, ".");
            node_to_last_segment.emplace(key, (int)segment.packet.header.fragment_num);
        }
        if (node_to_last_segment.contains(key) && node_to_last_segment[key] == node_to_received_segments.size() - 1)
        {
            log_debug("Received all segments from ", key, ".");
            Message m{origin: segment.origin, destination: segment.destination, data: buffer, size: node_to_message_size[key]};
            control.forward_receive(m);
            node_to_receive_buffer.erase(key);
            node_to_received_segments.erase(key);
            node_to_last_segment.erase(key);
        }
    }

private:
    // TODO: passar pra uma unica classe q faz esse controle
    std::map<std::string, std::array<char, MAXIMUM_MESSAGE_SIZE>> node_to_receive_buffer;
    std::map<std::string, std::vector<int>> node_to_received_segments;
    std::map<std::string, int> node_to_last_segment;
};
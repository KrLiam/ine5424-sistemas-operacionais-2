#include <thread>

#include "pipeline/pipeline_step.h"
#include "core/packet.h"
#include "utils/date.h"
#include "crc16.h"

class ChecksumLayer : public PipelineStep
{
    Node& local_node;

public:
    ChecksumLayer(PipelineHandler handler, Node& local_node);

    ~ChecksumLayer();

    void send(Packet packet);

    void receive(Packet packet);

private:
    void prepare_packet_buffer(const PacketData &packet_data, std::size_t message_length, char *buffer);
};
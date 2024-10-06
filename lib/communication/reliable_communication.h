#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <semaphore>

#include "communication/connection.h"
#include "core/buffer.h"
#include "core/constants.h"
#include "core/message.h"
#include "core/packet.h"
#include "core/node.h"
#include "communication/group_registry.h"
#include "pipeline/pipeline.h"
#include "utils/format.h"
#include "communication/transmission.h"
#include "pipeline/fault_injection/fault_injection_layer.h"
#include "failure_detection/failure_detection.h"

class Pipeline;

struct ReceiveResult {
    size_t length;
    size_t truncated_bytes;
    SocketAddress sender_address;
    std::string sender_id;
};

class ReliableCommunication
{
public:
    ReliableCommunication(std::string local_id, std::size_t user_buffer_size);
    ReliableCommunication(
        std::string local_id,
        std::size_t user_buffer_size,
        FaultConfig fault_config
    );
    ~ReliableCommunication();

    void shutdown();

    bool send(std::string id, MessageData data);
    bool broadcast(MessageData data);
    ReceiveResult receive(char *m);

    std::shared_ptr<GroupRegistry> get_group_registry();

private:
    Pipeline *pipeline;
    std::shared_ptr<GroupRegistry> gr;
    std::unique_ptr<FailureDetection> failure_detection;
    
    EventBus event_bus;

    std::thread sender_thread;
    BufferSet<std::string> connection_update_buffer;

    std::size_t user_buffer_size;
    Buffer<Message> application_buffer{"application receive"};

    Message create_message(std::string receiver_id, const MessageData& data);
    Message create_message(SocketAddress receiver_address, const MessageData& data);

    Transmission create_transmission(std::string receiver_id, const MessageData& data);
    Transmission create_transmission(SocketAddress receiver_address, const MessageData& data);

    void send_routine();
};

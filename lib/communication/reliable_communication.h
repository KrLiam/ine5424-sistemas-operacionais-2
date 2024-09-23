#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <semaphore>

#include "channels/channel.h"
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

class Pipeline;

struct MessageData
{
    const char *ptr;
    std::size_t size = -1;

    MessageData(const char *ptr) : ptr(ptr) {}
    MessageData(const char *ptr, std::size_t size) : ptr(ptr), size(size) {}
};

struct ReceiveResult {
    size_t bytes;
    size_t truncated_bytes;
    SocketAddress sender_address;
    std::string sender_id;
};

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);
    ReliableCommunication(
        std::string _local_id,
        std::size_t _user_buffer_size,
        FaultConfig fault_config
    );
    ~ReliableCommunication();

    bool send(std::string id, MessageData data);
    ReceiveResult receive(char *m);

    GroupRegistry *get_group_registry();

private:
    Channel *channel;

    Pipeline *pipeline;
    GroupRegistry *gr;

    std::thread sender_thread;
    BufferSet<std::string> connection_update_buffer;

    std::size_t user_buffer_size;
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> application_buffer{"application receive"};

    Message create_message(std::string id, const MessageData& data);
    Transmission create_transmission(std::string id, const MessageData& data);

    void enqueue(Transmission& transmission);

    void send_routine();
};

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

class Pipeline;

struct MessageData
{
    char *ptr;
    std::size_t size = -1;

    MessageData(char *ptr) : ptr(ptr) {}
    MessageData(char *ptr, std::size_t size) : ptr(ptr), size(size) {}
};

struct SendRequest {
    std::string id;
    MessageData data;
};

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);
    ~ReliableCommunication();

    void send(std::string id, MessageData data);
    Message receive(char *m);

    GroupRegistry *get_group_registry();

private:
    Channel *channel;

    Pipeline *pipeline;
    GroupRegistry *gr;

    bool alive = true;
    std::thread sender_thread;
    Buffer<INTERMEDIARY_BUFFER_ITEMS, std::string> connection_update_buffer{"connection_updates"};

    std::size_t user_buffer_size;
    Buffer<INTERMEDIARY_BUFFER_ITEMS, Message> application_buffer{"application receive"};

    Message create_message(std::string id, const MessageData& data);
    Transmission create_transmission(std::string id, const MessageData& data);

    void enqueue(Transmission& transmission);

    void send_routine();
};

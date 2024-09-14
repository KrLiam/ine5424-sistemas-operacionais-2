#pragma once

#include <cmath>
#include <map>
#include <memory>
#include <semaphore>

#include "channels/channel.h"
#include "core/buffer.h"
#include "core/constants.h"
#include "core/segment.h"
#include "core/node.h"
#include "communication/group_registry.h"
#include "communication/pipeline.h"
#include "utils/format.h"
#include "utils/hash.h"

class Pipeline;

class ReliableCommunication
{
public:
    ReliableCommunication(std::string _local_id, std::size_t _user_buffer_size);
    ~ReliableCommunication();

    void send(std::string id, char *m);
    Message receive(char *m);

    GroupRegistry *get_group_registry();

private:
    Channel *channel;

    Pipeline *pipeline;
    GroupRegistry *gr;

    std::size_t user_buffer_size;
};

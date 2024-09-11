#pragma once

#include <functional>

#include "core/segment.h"

class PipelineHandler
{
private:
    std::function<void(unsigned int, char*)> send_callback;
    std::function<void(unsigned int, char*)> receive_callback;
    std::function<void(Message)> application_forward_callback;

public:
    PipelineHandler(std::function<void(unsigned int, char *)> send_callback, std::function<void(unsigned int, char *)> receive_callback, std::function<void(Message)> application_forward_callback) : send_callback(send_callback), receive_callback(receive_callback), application_forward_callback(application_forward_callback) {}

    void send_to(unsigned int layer_number, char* m)
    {
        send_callback(layer_number, m);
    }

    void receive_on(unsigned int layer_number, char* m)
    {
        receive_callback(layer_number, m);
    }

    void forward_to_application(Message message)
    {
        application_forward_callback(message);
    }
};

#include "node.h"

Node::Node(const NodeConfig& _config, const bool& _remote)
{
    config = _config;
    remote = _remote;
}

Node::~Node()
{
}

const NodeConfig& Node::get_config() const
{
    return config;
}

const bool& Node::is_remote() const
{
    return remote;
}

std::string Node::to_string() const
{
    return std::format("{}:{}:{}", config.id, config.address.to_string(), remote);
}

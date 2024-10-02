#include "communication/group_registry.h"
#include "pipeline/pipeline.h"

GroupRegistry::GroupRegistry(std::string local_id) : local_id(local_id)
{
    read_nodes_from_configuration(local_id);
}

GroupRegistry::~GroupRegistry()
{
}

const NodeMap& GroupRegistry::get_nodes()
{
    return nodes;
}

const Node &GroupRegistry::get_local_node()
{
    return nodes.get_node(local_id);
}

void GroupRegistry::read_nodes_from_configuration(std::string local_id)
{
    nodes.clear();
    Config config = ConfigReader::parse_file("nodes.conf");
    for (NodeConfig node_config : config.nodes)
    {
        bool is_remote = local_id != node_config.id;
        Node node(node_config.id, node_config.address, is_remote);
        nodes.add(node);
    }
}

void GroupRegistry::establish_connections(
    Pipeline &pipeline,
    Buffer<Message> &application_buffer,
    BufferSet<std::string> &connection_update_buffer
) {
    Node local_node = get_local_node();
    for (auto &[id, node] : nodes)
        connections.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(local_node, node, pipeline, application_buffer, connection_update_buffer)
        );
}
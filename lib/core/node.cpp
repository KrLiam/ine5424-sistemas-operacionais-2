#include "core/node.h"
#include "utils/log.h"

Node::Node(std::string id, SocketAddress address, NodeState state, bool remote)
    : id(id), address(address), remote(remote), pid(rc_random::dis32(rc_random::gen)), state(state), leader(false), receiving_ab_broadcast(false) { };

Node::~Node()
{
}

const std::string &Node::get_id() const
{
    return id;
}

const SocketAddress &Node::get_address() const
{
    return address;
}

bool Node::is_remote() const
{
    return remote;
}

const std::set<uint64_t>& Node::get_groups() const { return groups; }

std::string Node::to_string() const
{
    return format("%s:%s:%i", id.c_str(), address.to_string().c_str(), remote);
}

bool Node::operator==(const Node& other) const
{
    return address == other.address;
}

NodeMap::NodeMap() : nodes() {}

NodeMap::NodeMap(std::map<std::string, Node> nodes) {
    for (const auto& [_, node] : nodes) {
        add(node);
    }
}

Node &NodeMap::get_node(std::string id)
{
    auto iterator = nodes.find(id);
    if (iterator != nodes.end())
    {
        return nodes.at(iterator->first);
    }
    throw std::invalid_argument(format("Node %s not found.", id.c_str()));
}

Node &NodeMap::get_node(const SocketAddress& address)
{
    for (auto &[id, node] : nodes)
    {
        if (address == node.get_address())
        {
            return node;
        }
    }
    throw std::invalid_argument(format("Node with address %s not found.", address.to_string().c_str()));
}

Node *NodeMap::get_leader() {
    for (auto &[id, node] : nodes)
    {
        if (node.is_leader()) return &node;
    }
    
    return nullptr;
}

Node *NodeMap::get_local() {
    for (auto &[id, node] : nodes)
    {
        if (!node.is_remote()) return &node;
    }
    
    return nullptr;
}

bool NodeMap::contains(const SocketAddress& address) const
{
    for (auto &[id, node] : nodes)
    {
        if (address == node.get_address())
        {
            return true;
        }
    }
    return false;
}

void NodeMap::add(const Node& node) {
    std::string id = node.get_id();
    nodes.emplace(id, node);

    // global group
    if (!groups.contains(0)) groups.emplace(0, std::unordered_set<Node*>());
    groups.at(0).emplace(&nodes.at(id));
}

bool NodeMap::contains_group(uint64_t hash) const {
    return groups.contains(hash);
}
const std::unordered_set<Node*>& NodeMap::get_group(uint64_t hash) {
    if (!groups.contains(hash)) groups.emplace(hash, std::unordered_set<Node*>());

    return groups.at(hash);
}

void NodeMap::update_groups(Node& node, const std::set<uint64_t>& new_groups) {
    if (node.groups == new_groups) return;

    for (uint64_t id : node.groups) {
        if (new_groups.contains(id)) continue;

        if (!groups.contains(id)) continue;
        auto& group_set = groups.at(id);

        group_set.erase(&node);
        log_info("Node ", node.get_id(), " left group of hash ", id, ".");
    }

    for (uint64_t id : new_groups) {
        if (node.groups.contains(id)) continue;

        if (!groups.contains(id)) groups.emplace(id, std::unordered_set<Node*>());
        auto& group_set = groups.at(id);

        group_set.emplace(&node);
        log_info("Node ", node.get_id(), " entered group of hash ", id, ".");
    }

    node.groups = new_groups;
}

std::map<std::string, Node>::iterator NodeMap::begin() {
    return nodes.begin();
}

std::map<std::string, Node>::iterator NodeMap::end() {
    return nodes.end();
}

std::map<std::string, Node>::const_iterator NodeMap::begin() const {
    return nodes.begin();
}

std::map<std::string, Node>::const_iterator NodeMap::end() const {
    return nodes.end();
}

void NodeMap::clear() {
    nodes.clear();
    groups.clear();
}

std::size_t NodeMap::size() {
    return nodes.size();
}
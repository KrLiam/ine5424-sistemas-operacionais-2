#include <memory>
#include <vector>

#include "command.h"
#include "utils/reader.h"
#include "utils/format.h"
#include "utils/log.h"
#include <communication/transmission.h>

Command::Command(CommandType type) : type(type) {}

TextCommand::TextCommand(std::string text, Destination dest)
    : Command(CommandType::text), text(text), dest(dest) {}

std::string TextCommand::name() const { return "text"; }

DummyCommand::DummyCommand(size_t size, Destination dest)
    : Command(CommandType::dummy), size(size), dest(dest) {}

std::string DummyCommand::name() const { return "dummy"; }

FileCommand::FileCommand(std::string path, Destination dest)
    : Command(CommandType::file), path(path), dest(dest)  {}

std::string FileCommand::name() const { return "file"; }

FaultCommand::FaultCommand(std::vector<FaultRule> rules)
    : Command(CommandType::fault), rules(rules) {}

std::string FaultCommand::name() const { return "fault"; }

KillCommand::KillCommand() : Command(CommandType::kill) {}

std::string KillCommand::name() const { return "kill"; }

InitCommand::InitCommand() : Command(CommandType::init) {}

std::string InitCommand::name() const { return "init"; }

SequenceCommand::SequenceCommand(const std::vector<std::shared_ptr<Command>>& subcommands)
    : Command(CommandType::sequence), subcommands(subcommands) {}

std::string SequenceCommand::name() const { return "sequence"; }

AsyncCommand::AsyncCommand(std::shared_ptr<Command> subcommand)
    : Command(CommandType::async), subcommand(subcommand) {}

std::string AsyncCommand::name() const { return "async"; }

SleepCommand::SleepCommand(int interval)
    : Command(CommandType::sleep), interval(interval) {}

std::string SleepCommand::name() const { return "sleep"; }

RepeatCommand::RepeatCommand(std::shared_ptr<Command> subcommand, int count)
    : Command(CommandType::repeat), count(count), subcommand(subcommand) {}

std::string RepeatCommand::name() const { return "repeat"; }


GroupListCommand::GroupListCommand() : Command(CommandType::group_list) {}

std::string GroupListCommand::name() const { return "group list"; }

GroupJoinCommand::GroupJoinCommand(std::string id, std::optional<ByteArray> key)
    : Command(CommandType::group_join), id(id), key(key) {}

std::string GroupJoinCommand::name() const { return "group join"; }

GroupLeaveCommand::GroupLeaveCommand(std::string id)
    : Command(CommandType::group_leave), id(id) {}

std::string GroupLeaveCommand::name() const { return "group leave"; }

NodeListCommand::NodeListCommand() : Command(CommandType::node_list) {}

std::string NodeListCommand::name() const { return "node list"; }


NodeDiscoverCommand::NodeDiscoverCommand(const SocketAddress& address)
    : Command(CommandType::node_discover), address(address) {}

std::string NodeDiscoverCommand::name() const { return "node discover"; }


std::string parse_string(Reader& reader) {
    reader.expect('"');
    Override ovr = reader.override_whitespace(false);

    std::string value;

    while (!reader.eof()) {
        char ch = reader.peek();

        if (!ch) break;
        if (ch == '"') break;

        value += ch;
        reader.advance();
    }

    reader.expect('"');

    return value;
}

std::string parse_path(Reader& reader) {
    char ch = reader.peek();

    if (ch == '"') return parse_string(reader);

    Override ovr = reader.override_whitespace(false);

    std::string value;

    while (ch && !isspace(ch)) {
        value += ch;
        reader.advance();

        ch = reader.peek();
    }

    return value;
}

Destination parse_destination(Reader& reader) {
    reader.expect("->");

    Destination dest;

    if (reader.read('*')) {
        dest.node = BROADCAST_ID;
    }
    else {
        std::string node_id = reader.read_word();
        if (!node_id.length()) throw std::invalid_argument(
            format("Expected send id after -> at pos ", reader.get_pos())
        );
        dest.node = node_id;
    }

    if (reader.read("on")) {
        dest.group = reader.read_word();
    }

    return dest;
}

FaultRule parse_fault_rule(Reader& reader) {
    std::string type = reader.read_word();

    if (type == "drop") {
        DropFaultRule rule{
            pattern: {number: IntRange::full(), fragment: IntRange::full(), sequence_types: {'u','b','a','h'}, flags: 0, source: ""},
            chance: 1.0,
            count: UINT32_MAX,
        };

        rule.pattern.number = IntRange::parse(reader);

        if (reader.read('/')) {
            rule.pattern.fragment = IntRange::parse(reader);
        }

        std::unordered_map<char, MessageSequenceType> ch_map = {
            {'u', MessageSequenceType::UNICAST},
            {'b', MessageSequenceType::BROADCAST},
            {'a', MessageSequenceType::ATOMIC},
            {'h', MessageSequenceType::HEARTBEAT}
        };

        {
            Override ovr = reader.override_whitespace(false);

            char ch = reader.peek();
            if (ch_map.contains(ch)) {
                reader.advance();
                rule.pattern.sequence_types = {(char)ch_map.at(ch)};
            }
        }

        if (reader.read("ack")) rule.pattern.flags |= ACK;

        if (reader.read("from")) {
            rule.pattern.source = reader.read('*') ? std::string("") : reader.read_word();
        }

        if (reader.peek() && isdigit(reader.peek())) {
            int value = reader.read_int();
            char value_unit = reader.peek();

            if (value_unit == '%') rule.chance = (double) value / 100;
            else if (value_unit == 'x') rule.count = value;
            else throw parse_error(format("Invalid drop rule unit '%c'", value_unit));

            reader.advance();
        }

        return rule;
    }

    throw parse_error(format("Unknown fault rule '%s'.", type.c_str()));
}
std::vector<FaultRule> parse_fault_rules(Reader& reader) {
    std::vector<FaultRule> rules;

    while (!reader.eof()) {
        rules.push_back(parse_fault_rule(reader));
        if (!reader.read(',')) break;
    }

    return rules;
}


std::shared_ptr<SequenceCommand> parse_command_list(Reader& reader) {
    reader.expect('[');
    std::vector<std::shared_ptr<Command>> commands = parse_commands(reader, ']');
    reader.expect(']');

    return std::make_shared<SequenceCommand>(commands);
}

std::shared_ptr<Command> parse_command(Reader& reader) {
    if (reader.peek() == '[') return parse_command_list(reader);

    std::string keyword = reader.read_word();

    if (keyword == "file") {
        std::string path = parse_path(reader);
        Destination dest = parse_destination(reader);
        return std::make_shared<FileCommand>(path, dest);
    }
    if (keyword == "text" || (!keyword.length() && reader.peek() == '"') ) {
        std::string text = parse_string(reader);
        Destination dest = parse_destination(reader);
        return std::make_shared<TextCommand>(text, dest);
    }
    if (keyword == "dummy") {
        int size = reader.read_int();
        Destination dest = parse_destination(reader);
        return std::make_shared<DummyCommand>(size, dest);
    }
    if (keyword == "kill") {
        return std::make_shared<KillCommand>();
    }
    if (keyword == "init") {
        return std::make_shared<InitCommand>();
    }
    if (keyword == "fault") {
        std::vector<FaultRule> rules = parse_fault_rules(reader);
        return std::make_shared<FaultCommand>(rules);
    }
    if (keyword == "async") {
        std::shared_ptr<Command> subcommand = parse_command(reader);
        return std::make_shared<AsyncCommand>(subcommand);
    }
    if (keyword == "sleep") {
        int interval = reader.read_int();
        return std::make_shared<SleepCommand>(interval);
    }
    if (keyword == "repeat") {
        int count = reader.read_int();
        std::shared_ptr<Command> subcommand = parse_command(reader);
        return std::make_shared<RepeatCommand>(subcommand, count);
    }
    if (keyword == "group") {
        int pos = reader.get_pos();
        std::string action = reader.read_word();

        if (action == "list") return std::make_shared<GroupListCommand>();
        if (action == "join") {
            std::string id = reader.read_word();

            std::optional<ByteArray> key =
                (reader.peek() == ';' || reader.eof()) ?
                std::nullopt :
                std::optional(Aes256::parse_hex_key(reader));

            return std::make_shared<GroupJoinCommand>(id, key);
        }
        if (action == "leave") {
            std::string id = reader.read_word();
            return std::make_shared<GroupLeaveCommand>(id);
        }

        throw parse_error(format("Invalid group command at pos %i", pos));
    }
    if (keyword == "node") {
        int pos = reader.get_pos();
        std::string action = reader.read_word();

        if (action == "list") return std::make_shared<NodeListCommand>();
        if (action == "discover") {
            SocketAddress address = SocketAddress::parse(reader);
            return std::make_shared<NodeDiscoverCommand>(address);
        }
    }

    if (keyword.length()) {
        throw std::invalid_argument(
            format("Unknown command '%s'.", keyword.c_str())
        );
    }

    char ch = reader.peek();
    throw std::invalid_argument(
        format(
            "Invalid character '%s' at pos %i.",
            ch ? std::to_string(ch).c_str() : "eof",
            reader.get_pos()
        )
    );
}

std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader) {
    return parse_commands(reader, 0);
}
std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader, char stop) {
    std::vector<std::shared_ptr<Command>> commands;

    while (!reader.eof()) {
        char ch = reader.peek();
        if (!ch || ch == stop) break;

        commands.push_back(parse_command(reader));

        if (!reader.read(';')) break;
    }

    return commands;
}

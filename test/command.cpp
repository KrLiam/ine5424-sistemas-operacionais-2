#include <memory>
#include <vector>

#include "command.h"
#include "utils/reader.h"
#include "utils/format.h"
#include "utils/log.h"
#include <communication/transmission.h>

Command::Command(CommandType type) : type(type) {}

TextCommand::TextCommand(std::string text, std::string send_id)
    : Command(CommandType::text), text(text), send_id(send_id) {}

std::string TextCommand::name() const { return "text"; }

BroadcastCommand::BroadcastCommand(std::string text)
    : Command(CommandType::broadcast), text(text) {}

std::string BroadcastCommand::name() const { return "broadcast"; }

DummyCommand::DummyCommand(size_t size, size_t count, std::string send_id)
    : Command(CommandType::dummy), size(size), count(count), send_id(send_id) {}

std::string DummyCommand::name() const { return "dummy"; }

FileCommand::FileCommand(std::string path, std::string send_id)
    : Command(CommandType::file), path(path), send_id(send_id)  {}

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
    : Command(CommandType::sleep_cmd), interval(interval) {}

std::string SleepCommand::name() const { return "sleep"; }

RepeatCommand::RepeatCommand(std::shared_ptr<Command> subcommand, int count)
    : Command(CommandType::repeat), count(count), subcommand(subcommand) {}

std::string RepeatCommand::name() const { return "repeat"; }


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

std::string parse_destination(Reader& reader) {
    reader.expect("->");

    if (reader.read('*')) return BROADCAST_ID;

    std::string send_id = reader.read_word();

    if (!send_id.length()) throw std::invalid_argument(
        format("Expected send id after -> at pos ", reader.get_pos())
    );

    return send_id;
}

FaultRule parse_fault_rule(Reader& reader) {
    std::string type = reader.read_word();

    if (type == "drop") {
        DropFaultRule rule{
            pattern: {number: IntRange::full(), fragment: IntRange::full()},
            chance: 1.0,
            count: UINT32_MAX
        };

        rule.pattern.number = IntRange::parse(reader);

        if (reader.read('/')) {
            rule.pattern.fragment = IntRange::parse(reader);
        }

        std::unordered_map<char, MessageSequenceType> ch_map = {
            {'u', MessageSequenceType::UNICAST},
            {'b', MessageSequenceType::BROADCAST},
            {'a', MessageSequenceType::ATOMIC}
        };

        rule.pattern.sequence_types = {'u','b','a'};
        char ch = reader.peek();
        if (ch_map.contains(ch)) {
            reader.advance();
            rule.pattern.sequence_types = {(char)ch_map.at(ch)};
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
        std::string send_id = parse_destination(reader);
        return std::make_shared<FileCommand>(path, send_id);
    }
    if (keyword == "text" || (!keyword.length() && reader.peek() == '"') ) {
        std::string text = parse_string(reader);
        std::string send_id = parse_destination(reader);
        return std::make_shared<TextCommand>(text, send_id);
    }
    if (keyword == "dummy") {
        int size = reader.read_int();
        size_t count = reader.read_int();
        std::string send_id = parse_destination(reader);
        return std::make_shared<DummyCommand>(size, count, send_id);
    }
    if (keyword == "broadcast") {
        std::string text = parse_string(reader);
        return std::make_shared<BroadcastCommand>(text);
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

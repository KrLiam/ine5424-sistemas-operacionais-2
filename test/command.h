#pragma once

#include <optional>

#include "utils/reader.h"
#include "pipeline/fault_injection/fault_injection_layer.h"
#include "communication/transmission.h"


enum class CommandType : char {
    text = 0,
    file = 1,
    dummy = 2,
    broadcast = 3,
    kill = 4,
    init = 5,
    fault = 6,
    sequence = 7,
    async = 8,
    sleep = 9,
    repeat = 10,
    group_list = 11,
    group_join = 12,
    group_leave = 13,
    node_list = 14
};


struct Destination {
    std::string node;
    std::string group = GLOBAL_GROUP_ID;
};

struct Command {
    CommandType type;
protected:
    Command(CommandType type);

    virtual std::string name() const = 0;
};

struct TextCommand : public Command {
    std::string text;
    Destination dest;

    TextCommand(std::string text, Destination dest);

    virtual std::string name() const;
};

struct DummyCommand : public Command {
    size_t size;
    Destination dest;

    DummyCommand(size_t size, Destination dest);

    virtual std::string name() const;
};

struct FileCommand : public Command {
    std::string path;
    Destination dest;

    FileCommand(std::string path, Destination dest);

    virtual std::string name() const;
};

struct FaultCommand : public Command {
    std::vector<FaultRule> rules;

    FaultCommand(std::vector<FaultRule> rules);

    virtual std::string name() const;
};

struct KillCommand : public Command {
    KillCommand();

    virtual std::string name() const;
};

struct InitCommand : public Command {
    InitCommand();

    virtual std::string name() const;
};

struct SequenceCommand : public Command {
    std::vector<std::shared_ptr<Command>> subcommands;

    SequenceCommand(const std::vector<std::shared_ptr<Command>>&);

    virtual std::string name() const;
};

struct AsyncCommand : public Command {
    std::shared_ptr<Command> subcommand;

    AsyncCommand(std::shared_ptr<Command>);

    virtual std::string name() const;
};

struct SleepCommand : public Command {
    int interval;

    SleepCommand(int interval);

    virtual std::string name() const;
};

struct RepeatCommand : public Command {
    int count;
    std::shared_ptr<Command> subcommand;

    RepeatCommand(std::shared_ptr<Command> subcommand, int count);

    virtual std::string name() const;
};

struct GroupListCommand : public Command {
    GroupListCommand();

    virtual std::string name() const;
};

struct GroupJoinCommand : public Command {
    GroupJoinCommand(std::string id, std::optional<ByteArray> key);

    std::string id;
    std::optional<ByteArray> key;

    virtual std::string name() const;
};

struct GroupLeaveCommand : public Command {
    GroupLeaveCommand(std::string name);

    std::string id;

    virtual std::string name() const;
};

struct NodeListCommand : public Command {
    NodeListCommand();

    virtual std::string name() const;
};


std::string parse_string(Reader& reader);

std::string parse_path(Reader& reader);

Destination parse_destination(Reader& reader);

std::shared_ptr<SequenceCommand> parse_command_list(Reader& reader);
std::shared_ptr<Command> parse_command(Reader& reader);

std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader);
std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader, char stop);
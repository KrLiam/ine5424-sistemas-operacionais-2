#pragma once

#include "utils/reader.h"
#include "pipeline/fault_injection/fault_injection_layer.h"


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
    repeat = 10
};

struct Command {
    CommandType type;
protected:
    Command(CommandType type);

    virtual std::string name() const = 0;
};

struct TextCommand : public Command {
    std::string text;
    std::string send_id;

    TextCommand(std::string text, std::string send_id);

    virtual std::string name() const;
};

struct BroadcastCommand : public Command {
    std::string text;

    BroadcastCommand(std::string text);

    virtual std::string name() const;
};

struct DummyCommand : public Command {
    size_t size;
    std::string send_id;

    DummyCommand(size_t size, std::string send_id);

    virtual std::string name() const;
};

struct FileCommand : public Command {
    std::string path;
    std::string send_id;

    FileCommand(std::string path, std::string send_id);

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

std::string parse_string(Reader& reader);

std::string parse_path(Reader& reader);

std::string parse_destination(Reader& reader);

std::shared_ptr<SequenceCommand> parse_command_list(Reader& reader);
std::shared_ptr<Command> parse_command(Reader& reader);

std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader);
std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader, char stop);
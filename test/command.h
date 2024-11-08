#pragma once

#include "utils/reader.h"
#include "pipeline/fault_injection/fault_injection_layer.h"


enum CommandType {
    text = 0,
    file = 1,
    dummy = 2,
    broadcast = 3,
    kill = 4,
    init = 5,
    fault = 6
};

struct Command {
    CommandType type;
protected:
    Command(CommandType type);

    virtual std::string name() = 0;
};

struct TextCommand : public Command {
    std::string text;
    std::string send_id;

    TextCommand(std::string text, std::string send_id);

    virtual std::string name();
};

struct BroadcastCommand : public Command {
    std::string text;

    BroadcastCommand(std::string text);

    virtual std::string name();
};

struct DummyCommand : public Command {
    size_t size;
    size_t count;
    std::string send_id;

    DummyCommand(size_t size, size_t count, std::string send_id);

    virtual std::string name();
};

struct FileCommand : public Command {
    std::string path;
    std::string send_id;

    FileCommand(std::string path, std::string send_id);

    virtual std::string name();
};

struct FaultCommand : public Command {
    std::vector<FaultRule> rules;

    FaultCommand(std::vector<FaultRule> rules);

    virtual std::string name();
};

struct KillCommand : public Command {
    KillCommand();

    virtual std::string name();
};

struct InitCommand : public Command {
    InitCommand();

    virtual std::string name();
};


std::string parse_string(Reader& reader);

std::string parse_path(Reader& reader);

std::string parse_destination(Reader& reader);

std::shared_ptr<Command> parse_command(Reader& reader);

std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader);
std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader, char stop);
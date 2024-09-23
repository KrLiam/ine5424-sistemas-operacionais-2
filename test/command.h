
#include "utils/reader.h"


enum CommandType {
    text = 0,
    file = 1
};

struct Command {
    CommandType type;
protected:
    Command(CommandType type);
};

struct TextCommand : public Command {
    std::string text;
    std::string send_id;

    TextCommand(std::string text, std::string send_id);
};

struct FileCommand : public Command {
    std::string path;
    std::string send_id;

    FileCommand(std::string path, std::string send_id);
};


std::string parse_string(Reader& reader);

std::string parse_path(Reader& reader);

std::string parse_destination(Reader& reader);

std::shared_ptr<Command> parse_command(Reader& reader);

std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader);
std::vector<std::shared_ptr<Command>> parse_commands(Reader& reader, char stop);
#include <memory>
#include <vector>

#include "command.h"
#include "utils/reader.h"
#include "utils/format.h"
#include "utils/log.h"

Command::Command(CommandType type) : type(type) {}

TextCommand::TextCommand(std::string text, std::string send_id)
    : Command(CommandType::text), text(text), send_id(send_id) {}

std::string TextCommand::name() { return "text"; }

DummyCommand::DummyCommand(size_t size, std::string send_id)
    : Command(CommandType::dummy), size(size), send_id(send_id) {}

std::string DummyCommand::name() { return "dummy"; }

FileCommand::FileCommand(std::string path, std::string send_id)
    : Command(CommandType::file), path(path), send_id(send_id)  {}

std::string FileCommand::name() { return "file"; }


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
    std::string send_id = reader.read_word();

    if (!send_id.length()) throw std::invalid_argument(
        format("Expected send id after -> at pos ", reader.get_pos())
    );

    return send_id;
}

std::shared_ptr<Command> parse_command(Reader& reader) {
    std::string keyword = reader.read_word();

    if (keyword == "file") {
        std::string path = parse_path(reader);
        std::string send_id = parse_destination(reader);
        return std::make_shared<FileCommand>(path, send_id);
    }
    if (keyword == "text" || reader.peek() == '"') {
        std::string text = parse_string(reader);
        std::string send_id = parse_destination(reader);
        return std::make_shared<TextCommand>(text, send_id);
    }
    if (keyword == "dummy") {
        int size = reader.read_int();

        std::string send_id = parse_destination(reader);
        return std::make_shared<DummyCommand>(size, send_id);
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
        if (ch == stop) break;

        commands.push_back(parse_command(reader));

        if (!reader.read(';')) break;
    }

    return commands;
}
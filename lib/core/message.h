#pragma once

#include "utils/config.h"
#include "utils/format.h"
#include "utils/uuid.h"
#include "constants.h"
#include <cstring>
#include <cstdint>

enum class MessageType : uint8_t
{
    // Padrão
    // <type0><type1><type2><type3><type4><broadcast><data><application>
    // type: Tipo da mensagem.
    // broadcast: Se a mensagem é broadcast.
    // application: Se a mensagem deve ser entregue à aplicação.
    // data: Se a mensagem/pacote contém dados que devem sofrer desfragmentação. Ou seja,
    //       ao receber pacotes com esta flag ativada, a camada de fragmentação
    //       irá desfragmentar e produzir uma mensagem.

    // Mensagens da lib
    CONTROL    = 0b00000000,
    HEARTBEAT  = 0b00001000,
    RAFT       = 0b00000100,
    AB_REQUEST = 0b00000010,
    // Mensagens da aplicação
    SEND       = 0b00000011,
    BEB        = 0b00000111,
    URB        = 0b00001111,
    AB_URB     = 0b00010111,
};
// queria tanto que enums pudessem ter métodos :(
namespace message_type {
    MessageType from_broadcast_type(BroadcastType type);
    bool is_application(MessageType type);
    bool is_data(MessageType type);
    bool is_broadcast(MessageType type);
    bool is_urb(MessageType type);
    bool is_atomic(MessageType type);
};

struct MessageIdentity {
    // Endereço do nó original que envia a mensagem.
    SocketAddress origin;
    uint32_t msg_num;
    MessageType msg_type;

    bool operator==(const MessageIdentity& other) const;
};

template<> struct std::hash<MessageIdentity> {
    std::size_t operator()(const MessageIdentity& id) const {
        return std::hash<SocketAddress>()(id.origin)
            ^ std::hash<uint32_t>()(id.msg_num)
            ^ std::hash<MessageType>()(id.msg_type);
    }
};


struct MessageData
{
    const char *ptr;
    std::size_t size = -1;

    MessageData(const char *ptr) : ptr(ptr) {}
    MessageData(const char *ptr, std::size_t size) : ptr(ptr), size(size) {}

    template <typename T>
    static MessageData from(T& data)
    {
        return MessageData(reinterpret_cast<char*>(&data), sizeof(T));
    }
};

struct Message
{
    inline const static int MAX_SIZE = 65536;

    MessageIdentity id;
    UUID transmission_uuid;
    // Endereço do processo que envia a mensagem. Pode ser diferente
    // do MessageIdentity::origin no caso de retransmissão do URB.
    SocketAddress origin;
    // Endereço do processo que recebe a mensagem.
    SocketAddress destination;

    char data[MAX_SIZE];
    std::size_t length;

    bool is_application() {
        return message_type::is_application(id.msg_type);
    }

    std::string to_string() const
    {
        return format("%s -> %s", id.origin.to_string().c_str(), destination.to_string().c_str());
    }

};

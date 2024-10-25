#pragma once

#include "core/message.h"


const uint8_t ACK = 0b10000000;
const uint8_t RST = 0b01000000;
const uint8_t SYN = 0b00100000;
const uint8_t FIN = 0b00010000;
const uint8_t END = 0b00001000;
const uint8_t RVO = 0b00000100;
const uint8_t LDR = 0b00000010;

struct PacketHeader
{
    MessageIdentity id;
    uint32_t fragment_num;
    uint16_t checksum;
    uint8_t flags;

    uint32_t get_message_number() const
    {
        return id.msg_num;
    }

    uint32_t get_fragment_number() const
    {
        return (uint32_t)fragment_num;
    }

    bool is_data() const {
        return message_type::is_application(id.msg_type) && !is_ack();
    }

    bool is_ack() const
    {
        return flags & ACK;
    }

    bool is_end() const
    {
        return flags & END;
    }

    bool is_syn() const
    {
        return flags & SYN;
    }

    bool is_rst() const
    {
        return flags & RST;
    }

    bool is_fin() const
    {
        return flags & FIN;
    }

    bool is_rvo() const
    {
        return flags & RVO;
    }

    bool is_ldr() const
    {
        return flags & LDR;
    }

    MessageType get_message_type() const
    {
        return static_cast<MessageType>(id.msg_type);
    }
};

struct PacketData
{
    static const int MAX_PACKET_SIZE = 1280;
    static const int MAX_MESSAGE_SIZE = MAX_PACKET_SIZE - sizeof(PacketHeader);

    PacketHeader header;
    char message_data[MAX_MESSAGE_SIZE];
};

struct PacketMetadata
{
    UUID transmission_uuid{""};
    // Endereço do processo que envia o pacote. Pode ser diferente
    // do MessageIdentity::origin no caso de retransmissão do URB.
    SocketAddress origin = {{0, 0, 0, 0}, 0};
    // Endereço do processo que recebe o pacote.
    SocketAddress destination = {{0, 0, 0, 0}, 0};
    int message_length = 0;
    bool expects_ack = 0;
    bool urb_retransmission = 0;
    bool silent = 0;
};


enum PacketFormat {
    ALL = 0,
    SENT = 1,
    RECEIVED = 2
};

struct Packet
{
    PacketData data;
    PacketMetadata meta;

    bool silent() { return meta.silent; }

    std::string to_string(PacketFormat type = PacketFormat::ALL) const
    {
        const PacketHeader& header = data.header;

        std::string flags;
        if (header.is_data()) flags += "DATA";
        if (header.is_syn()) flags += flags.length() ? "+SYN" : "SYN";
        if (header.is_rst()) flags += flags.length() ? "+RST" : "RST";
        if (header.is_fin()) flags += flags.length() ? "+FIN" : "FIN";
        if (header.is_ack()) flags += flags.length() ? "+ACK" : "ACK";
        if (header.is_end()) flags += flags.length() ? "+END" : "END";
        if (header.is_rvo()) flags += flags.length() ? "+RVO" : "RVO";
        if (header.is_ldr()) flags += flags.length() ? "+LDR" : "LDR";

        std::string origin = meta.origin.to_string();
        std::string destination = meta.destination.to_string();

        char sequence_type = message_type::is_broadcast(header.get_message_type()) ? 'b' : 'u';

        if (type == PacketFormat::RECEIVED) {
            return format(
                "%s %u/%u%c from %s",
                flags.c_str(),
                data.header.id.msg_num,
                data.header.fragment_num,
                sequence_type,
                origin.c_str()
            );
        }
        
        if (type == PacketFormat::SENT) {
            return format(
                "%s %u/%u%c to %s",
                flags.c_str(),
                data.header.id.msg_num,
                data.header.fragment_num,
                sequence_type,
                destination.c_str()
            );
        }

        return format(
            "%u %s %u/%u%c from %s to %s",
            header.flags,
            flags.c_str(),
            data.header.id.msg_num,
            data.header.fragment_num,
            sequence_type,
            origin.c_str(),
            destination.c_str()
        );
    }

    bool operator==(const Packet &other) const
    {
        return meta.destination == other.meta.destination
            && meta.message_length == other.meta.message_length
            && data.header.id == other.data.header.id
            && data.header.fragment_num == other.data.header.fragment_num;
    }
};

struct SynData {
    uint32_t broadcast_number;
    uint32_t ab_number;
};

struct RaftRPCData {
    bool success;
};


Packet create_ack(const Packet& packet);
Packet create_ack(const Packet& packet, SocketAddress destination);

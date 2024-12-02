#pragma once

#include <vector>

#include "utils/reader.h"

typedef std::vector<unsigned char> ByteArray;

template<> struct std::hash<ByteArray> {
    std::size_t operator()(const ByteArray &value) {
        const char* data = reinterpret_cast<const char*>(value.data());
        std::size_t size = value.size() * sizeof(value[0]);
        return std::hash<std::string_view>()(std::string_view(data, size));
    }
};

uint8_t hex_to_int(char ch);
char int_to_hex(uint8_t ch);

std::string array_to_hex(ByteArray value);


#define BLOCK_SIZE 16

class Aes256 {

    public:
        Aes256(const ByteArray& key);
        ~Aes256();

        static ByteArray parse_hex_key(Reader&);

        static ByteArray::size_type encrypt(const ByteArray& key, const ByteArray& plain, ByteArray& encrypted);
        static ByteArray::size_type encrypt(const ByteArray& key, const unsigned char* plain, const ByteArray::size_type plain_length, ByteArray& encrypted);
        static ByteArray::size_type decrypt(const ByteArray& key, const ByteArray& encrypted, ByteArray& plain);
        static ByteArray::size_type decrypt(const ByteArray& key, const unsigned char* encrypted, const ByteArray::size_type encrypted_length, ByteArray& plain);

        ByteArray::size_type encrypt_start(const ByteArray::size_type plain_length, ByteArray& encrypted);
        ByteArray::size_type encrypt_continue(const ByteArray& plain, ByteArray& encrypted);
        ByteArray::size_type encrypt_continue(const unsigned char* plain, const ByteArray::size_type plain_length, ByteArray& encrypted);
        ByteArray::size_type encrypt_end(ByteArray& encrypted);

        ByteArray::size_type decrypt_start(const ByteArray::size_type encrypted_length);
        ByteArray::size_type decrypt_continue(const ByteArray& encrypted, ByteArray& plain);
        ByteArray::size_type decrypt_continue(const unsigned char* encrypted, const ByteArray::size_type encrypted_length, ByteArray& plain);
        ByteArray::size_type decrypt_end(ByteArray& plain);

    private:
        ByteArray            m_key;
        ByteArray            m_salt;
        ByteArray            m_rkey;

        unsigned char        m_buffer[3 * BLOCK_SIZE];
        unsigned char        m_buffer_pos;
        ByteArray::size_type m_remainingLength;

        bool                 m_decryptInitialized;

        void check_and_encrypt_buffer(ByteArray& encrypted);
        void check_and_decrypt_buffer(ByteArray& plain);

        void encrypt(unsigned char *buffer);
        void decrypt(unsigned char *buffer);

        void expand_enc_key(unsigned char *rc);
        void expand_dec_key(unsigned char *rc);

        void sub_bytes(unsigned char *buffer);
        void sub_bytes_inv(unsigned char *buffer);

        void copy_key();

        void add_round_key(unsigned char *buffer, const unsigned char round);

        void shift_rows(unsigned char *buffer);
        void shift_rows_inv(unsigned char *buffer);

        void mix_columns(unsigned char *buffer);
        void mix_columns_inv(unsigned char *buffer);
};

#pragma once

#define INTERMEDIARY_BUFFER_ITEMS 100
#define MAX_ENQUEUED_TRANSMISSIONS 100

// TODO: o ACK_TIMEOUT tem que ser definido com base na configuração de delay
#define ACK_TIMEOUT 1250
#define HANDSHAKE_TIMEOUT 5000
#define MAX_PACKET_TRIES 6

#define MAX_HEARTBEAT_TRIES 6

#define DATA_DIR ".data"
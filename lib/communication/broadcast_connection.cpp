
#include "communication/broadcast_connection.h"

BroadcastConnection::BroadcastConnection(
    std::map<std::string, Connection>& connections,
    BufferSet<std::string>& connection_update_buffer
) : connections(connections), connection_update_buffer(connection_update_buffer) {}

bool BroadcastConnection::enqueue(Transmission& transmission) {
    transmissions.push_back(&transmission);
    connection_update_buffer.produce(BROADCAST_ID);

    return true;
}

void BroadcastConnection::update() {
}
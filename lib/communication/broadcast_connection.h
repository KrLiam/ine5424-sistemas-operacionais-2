
#include "communication/connection.h"
#include "communication/transmission.h"

class BroadcastConnection {
    std::map<std::string, Connection>& connections;

    Transmission* active_transmission = nullptr;
    std::vector<Transmission*> transmissions;

    BufferSet<std::string>& connection_update_buffer;

public:
    BroadcastConnection(
        std::map<std::string, Connection>& connections,
        BufferSet<std::string>& connection_update_buffer
    );

    bool enqueue(Transmission& transmission);

    void update();
};

#include "core/connection.h"
#include "communication/pipeline.h"

void Connection::send(Packet packet)
{
    pipeline.send(packet);
}
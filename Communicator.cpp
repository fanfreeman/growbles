#include "Communicator.h"
#include "assert.h"

Communicator::Communicator(CommunicatorMode mode) : mMode(mode)
{
}

void
Communicator::SetServer(const char* server)
{
    assert(mMode == COMMUNICATOR_MODE_CLIENT);
}

void
Communicator::SetNumClients(unsigned n)
{
    assert(mMode == COMMUNICATOR_MODE_SERVER);
}

void
Communicator::Connect()
{
}

/*
	clientsocket.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a client socket object

*/

#include "clientsocket.h"

MFDServiceClientSocket::MFDServiceClientSocket(int identifier, int socket_id, Type type)
                    :QSocketDevice(socket_id, type)
{
    unique_identifier = identifier;
    reading_state = false;
    setBlocking(false);
}                     


int MFDServiceClientSocket::getIdentifier()
{
    return unique_identifier;
}

void MFDServiceClientSocket::lockReadMutex()
{
    read_mutex.lock();
}

void MFDServiceClientSocket::lockWriteMutex()
{
    write_mutex.lock();
}

void MFDServiceClientSocket::unlockReadMutex()
{
    read_mutex.unlock();
}

void MFDServiceClientSocket::unlockWriteMutex()
{
    write_mutex.unlock();
}

void MFDServiceClientSocket::setReading(bool on_or_off)
{
    reading_state_mutex.lock();
        reading_state = on_or_off;
    reading_state_mutex.unlock();
}

bool MFDServiceClientSocket::isReading()
{
    bool return_value;
    reading_state_mutex.lock();
        return_value = reading_state;
    reading_state_mutex.unlock();
    return return_value;
}


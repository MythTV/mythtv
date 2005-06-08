/*
	clienttracker.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Tiny class to keep track of which clients are on which generation of
	data
*/

#include "clienttracker.h"

ClientTracker::ClientTracker(int l_client_id, int l_generation)
{
    client_id = l_client_id;
    generation = l_generation;
}

int ClientTracker::getClientId()
{
    return client_id;
}

int ClientTracker::getGeneration()
{
    return generation;
}

void ClientTracker::setGeneration(int new_generation)
{
    generation = new_generation;
}

ClientTracker::~ClientTracker()
{
}

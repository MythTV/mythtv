/*
	session.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Very closely based on:
	
	daapd 0.2.1, a server for the DAA protocol
	(c) deleet 2003, Alexander Oberdoerster
	
	Methods for daap sessions
*/

#include <iostream>
using namespace std;

#include "session.h"

u32  DaapSessions::getNewId()
{
    u32 session_id = random();
    
    sessions_mutex.lock();
    while(session_ids.contains(session_id))
    {
        session_id = random(); 
        cerr << "out of a random universe of numbers, you actually got two session ids that are the same" << endl;
    }
    
    session_ids.append(session_id);
    sessions_mutex.unlock();
    return session_id;
}

bool DaapSessions::isValid( const u32 session_id)
{
    sessions_mutex.lock();
        uint occurences = session_ids.contains(session_id);
    sessions_mutex.unlock();
    if(occurences > 0)
    {
        return true;
    }
    return false;
}

bool DaapSessions::erase( const u32 session_id )
{
    sessions_mutex.lock();
        uint how_many = session_ids.remove(session_id);
    sessions_mutex.unlock();
    if(how_many < 1)
    {
        cerr << "something tried to remove a session id that does not exist" << endl;
        return false;
    }
    else if(how_many > 1)
    {
        cerr << "wow ... you had multiple session ids with the same value in the list (they were all removed" << endl;
        return false;
    }
    return true;
}
	
DaapSessions::~DaapSessions()
{
}

/*
	mdcapsession.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for little object to generate mdcap session id's
*/

#include <iostream>
using namespace std;

#include "mdcapsession.h"

uint32_t  MdcapSessions::getNewId()
{
    uint32_t session_id = random();
    
    sessions_mutex.lock();
        while(session_ids.contains(session_id))
        {
            session_id = random(); 
            cerr << "out of a random universe of numbers, "
                 << "you actually got two session ids that "
                 << "are the same" 
                 << endl;
        }
        session_ids.append(session_id);
    sessions_mutex.unlock();

    return session_id;
}

bool MdcapSessions::isValid( const uint32_t session_id)
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

bool MdcapSessions::erase( const uint32_t session_id )
{
    sessions_mutex.lock();
        uint how_many = session_ids.remove(session_id);
    sessions_mutex.unlock();
    if(how_many < 1)
    {
        cerr << "something tried to remove a session id "
             << "that does not exist" 
             << endl;

        return false;
    }
    else if(how_many > 1)
    {
        cerr << "wow ... you had multiple session ids "
             << "with the same value in the list "
             << "(they were all removed)" 
             << endl;
        return true;
    }
    return true;
}
	
MdcapSessions::~MdcapSessions()
{
}

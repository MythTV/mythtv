#ifndef SESSION_H_
#define SESSION_H_
/*
	session.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Very closely based on:
	
	daapd 0.2.1, a server for the DAA protocol
	(c) deleet 2003, Alexander Oberdoerster
	
	Headers for daap sessions
*/

#include <qvaluelist.h>

#include "./daaplib/basic.h"

class DaapSessions
{

  public:

	DaapSessions() {}
    ~DaapSessions();

    u32  getNewId();
	bool isValid( const u32 session_id );
	bool erase( const u32 session_id );

  private:

    QValueList<int> session_ids;
};

#endif

#ifndef MDCAPSESSION_H_
#define MDCAPSESSION_H_
/*
	mdcapsession.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for little object to generate mdcap session id's
*/

#include <stdint.h>
#include <qvaluelist.h>
#include <qmutex.h>

class MdcapSessions
{

  public:

	MdcapSessions() {}
    ~MdcapSessions();

    uint32_t    getNewId();
	bool        isValid( const uint32_t session_id );
	bool        erase( const uint32_t session_id );

  private:

    QMutex sessions_mutex;
    QValueList<uint32_t> session_ids;
};

#endif

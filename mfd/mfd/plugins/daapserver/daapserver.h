#ifndef DAAPSERVER_H_
#define DAAPSERVER_H_
/*
	daapserver.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the daap server
*/

#include "../../mfdplugin.h"

class DaapServer: public MFDServicePlugin
{

  public:

    DaapServer(MFD *owner, int identity);
    void    run();
        
};

#endif  // daapserver_h_

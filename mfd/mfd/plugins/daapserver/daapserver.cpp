/*
	daapserver.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	

*/

#include "daapserver.h"

#include "httpd_persistent/httpd.h"

DaapServer::DaapServer(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 2343)
{
    
}

void DaapServer::run()
{
    //
    //  Create an httpd server (hey .. wadda ya need Apache for?)
    //

    httpd *server;
    server = httpdCreate(NULL, 3689);

    while(keep_going)
    {
        //  cout << "daap server exists" << endl;
        sleep(1);
    }
}


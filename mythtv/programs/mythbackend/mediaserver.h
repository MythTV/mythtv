//////////////////////////////////////////////////////////////////////////////
// Program Name: mediaserver.h
//                                                                            
// Purpose - uPnp Media Server main Class
//                                                                            
// Created By  : David Blain                    Created On : Jan. 15, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __MEDIASERVER_H__
#define __MEDIASERVER_H__

#include <qobject.h>
#include <qmutex.h>

#include "libmythupnp/upnp.h"

#include "libmythupnp/upnpcds.h"
#include "libmythupnp/upnpcmgr.h"
#include "libmythupnp/upnpmsrr.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MediaServer : public UPnp
{

    protected:
        
        UPnpCDS         *m_pUPnpCDS;      // Do not delete (auto deleted)
        UPnpCMGR        *m_pUPnpCMGR;     // Do not delete (auto deleted)

    public:
                 MediaServer( bool bMaster, bool bDisableUPnp = FALSE );
        virtual ~MediaServer();

//        void     customEvent( QCustomEvent *e );

        void     RegisterExtension  ( UPnpCDSExtension    *pExtension );
        void     UnregisterExtension( UPnpCDSExtension    *pExtension );

};

#endif

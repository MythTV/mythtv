//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.h
//                                                                            
// Purpose - Myth XML protocol client
//                                                                            
// Created By  : David Blain                    Created On : Mar. 19, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHXMLCLIENT_H_
#define MYTHXMLCLIENT_H_

#include <qdom.h>
#include <qbuffer.h>

#include "mythcontext.h"
#include "httpcomms.h"

#include "upnp.h"
#include "soapclient.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class MythXMLClient : public SOAPClient
{
    protected:

        bool    m_bInQtThread;

    public:

                 MythXMLClient( const QUrl &url, bool bInQtThread = true );
        virtual ~MythXMLClient( );
        
        UPnPResultCode GetConnectionInfo( const QString &sPin, DatabaseParams *pParams );

        // GetServiceDescription
        // GetProgramGuide      
        // GetHosts             
        // GetKeys              
        // GetSetting           
        // PutSetting           
        // GetChannelIcon       
        // GetRecorded          
        // GetPreviewImage      
        // GetRecording         
        // GetMusic             
        // GetExpiring          
        // GetProgramDetails    
        // GetVideo             

};

#endif


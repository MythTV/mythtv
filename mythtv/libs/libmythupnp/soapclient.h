//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.h
//                                                                            
// Purpose - SOAP client base class
//                                                                            
// Created By  : David Blain                    Created On : Mar. 19, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef SOAPCLIENT_H_
#define SOAPCLIENT_H_

#include <qdom.h>
#include <qbuffer.h>

#include "httpcomms.h"
#include "upnputil.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SOAPClient
{
    protected:

        QString m_sNamespace;
        QString m_sControlPath;
        QUrl    m_url;

    public:

                 SOAPClient( const QUrl    &url,
                             const QString &sNamespace,
                             const QString &sControlPath );
        virtual ~SOAPClient();

    protected:

        int     GetNodeValue( QDomNode &node, const QString &sName, int  nDefault );
        bool    GetNodeValue( QDomNode &node, const QString &sName, bool bDefault );
        QString GetNodeValue( QDomNode &node, const QString &sName, const QString &sDefault );

        QDomNode FindNode( const QString &sName , QDomNode &baseNode );
        QDomNode FindNode( QStringList   &sParts, QDomNode &curNode  );

        bool    SendSOAPRequest( const QString    &sMethod,
                                       QStringMap &list,
                                       int        &nErrCode,
                                       QString    &sErrDesc,
                                       bool        bInQtThread );
};

#endif


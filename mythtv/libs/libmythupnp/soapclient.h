//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.h
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SOAPCLIENT_H_
#define SOAPCLIENT_H_

#include <QDomDocument>
#include <QString>
#include <QUrl>

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

        QDomDocument SendSOAPRequest( const QString    &sMethod,
                                            QStringMap &list,
                                            int        &nErrCode,
                                            QString    &sErrDesc,
                                            bool        bInQtThread );
};

#endif


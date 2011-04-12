//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.h
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
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

#ifndef _WSDL_H_
#define _WSDL_H_

#include <QMetaObject>
#include <QMetaMethod>
#include <QString>
#include <QMap>
#include <QDomDocument>

#include "upnpexp.h"
#include "upnp.h"

#include "servicehost.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC Wsdl : public QDomDocument
{
    protected:
        ServiceHost             *m_pServiceHost;
        QMap< QString, bool >    m_typesCreated;

        QDomElement              m_oRoot;
        QDomElement              m_oTypes;
        QDomElement              m_oLastMsg;
        QDomElement              m_oPortType; 
        QDomElement              m_oBindings; 
        QDomElement              m_oService;  

    protected:

        QString     ConvertTypeToXSD( const QString &sType, bool bCustomType = false );

        QDomElement CreateBindingOperation( MethodInfo    &oInfo,
                                            const QString &sClassName );

        QDomElement CreateMessage         ( MethodInfo   &oInfo,
                                            QString       sMsgName, 
                                            QString       sTypeName );

        QDomElement CreateMethodType      ( MethodInfo   &oInfo,
                                            QString       sTypeName,
                                            bool          bReturnType = false );

        bool        CreateType            ( QObject       *pParent,
                                            QString       &sTypeName );

        bool        CreateArrayType       ( QObject       *pParent,
                                            QString      &sTypeName, 
                                            QString       sElementName,
                                            QString       sElementType,
                                            bool          bCustomType );

        bool        CreateMapType         ( QObject       *pParent,
                                            QString      &sTypeName, 
                                            QString       sElementName,
                                            QString       sElementType,
                                            bool          bCustomType );

        QString     ReadClassInfo         ( QObject *pObject, 
                                            QString  sKey );

    public:

        Wsdl( ServiceHost *pServiceHost );

        bool GetWSDL( HTTPRequest *pRequest );

};

#endif
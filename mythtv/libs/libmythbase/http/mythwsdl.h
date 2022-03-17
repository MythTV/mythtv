//////////////////////////////////////////////////////////////////////////////
// Program Name: wsdl.h
// Created     : Jan. 19, 2010
//
// Purpose     : WSDL XML Generation Class
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHWSDL_H
#define MYTHWSDL_H

#include <QMetaObject>
#include <QMetaMethod>
#include <QString>
#include <QMap>
#include <QDomDocument>

#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/http/mythhttptypes.h"
#include "libmythbase/http/mythxsd.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MythWSDL : public QDomDocument
{
    protected:
        // ServiceHost             *m_pServiceHost {nullptr};
        MythHTTPMetaService        *m_pMetaService {nullptr};
        QMap<QString, TypeInfo>  m_typesToInclude;

        QDomElement              m_oRoot;
        QDomElement              m_oTypes;
        QDomElement              m_oLastMsg;
        QDomElement              m_oPortType;
        QDomElement              m_oBindings;
        QDomElement              m_oService;

    protected:
        QDomElement CreateBindingOperation( const QString& path,
                                            const HTTPMethodPtr& handler,
                                            const QString &sClassName );
        QDomElement CreateMessage         ( const QString& sMsgName,
                                            const QString& sTypeName );
        QDomElement CreateMethodType      ( const HTTPMethodPtr& handler,
                                            QString       sTypeName,
                                            bool          bReturnType = false );
        static bool        IsCustomType          ( const QString &sTypeName );
        static QString     ReadClassInfo         ( const QMetaObject *pMeta,
                                            const QString     &sKey );
        QString     AddTypeInfo           ( QString            sType );

    public:
        explicit MythWSDL( MythHTTPMetaService *pMetaService )
            : m_pMetaService (pMetaService) {}
        HTTPResponse GetWSDL( const HTTPRequest2& Request );
};

#endif // WSDL_H

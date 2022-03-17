
//////////////////////////////////////////////////////////////////////////////
// Program Name: xsd.h
// Created     : Feb. 20, 2012
//
// Purpose     : XSD Generation Class
//
// Copyright (c) 2012 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHXSD_H
#define MYTHXSD_H

#include <QMetaObject>
#include <QString>
#include <QMap>
#include <QDomDocument>

#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/http/mythhttptypes.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class MythXSD : public QDomDocument
{
    protected:

        static QString     ReadPropertyMetadata ( QObject *pObject,
                                           const QString&  sPropName,
                                           const QString&  sKey );

        bool        RenderXSD            ( const HTTPRequest2& pRequest,
                                           QObject     *pClass );

        bool        RenderArrayXSD       ( const HTTPRequest2& pRequest,
                                           const QString &sClassName,
                                           bool           bCustomType );

        bool        RenderMapXSD         ( const HTTPRequest2& pRequest,
                                           const QString &sClassName,
                                           bool           bCustomType );


        QDomElement CreateSchemaRoot     ();

        QDomElement CreateComplexTypeNode( QMetaObject *pMetaObject );

        static bool        IsNillable           ( const QString       &sType );
        static bool        IsEnum               ( const QMetaProperty &metaProperty,
                                           const QString       &sType );

        static HTTPResponse Error(const HTTPRequest2& pRequest, const QString &msg);
    public:

        HTTPResponse GetXSD    ( const HTTPRequest2& pRequest, QString sTypeName );
        // HTTPResponse GetEnumXSD( const HTTPRequest2& pRequest, const QString& sEnumName );

        static QString ConvertTypeToXSD( const QString &sType, bool bCustomType = false );
};

//////////////////////////////////////////////////////////////////////////////

struct TypeInfo { QString sAttrName; QString sContentType; };

#endif // MYTHXSD_H

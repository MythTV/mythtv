//////////////////////////////////////////////////////////////////////////////
// Program Name: xsd.h
// Created     : Feb. 20, 2012
//
// Purpose     : XSD Generation Class 
//                                                                            
// Copyright (c) 2012 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef XSD_H
#define XSD_H

#include <QMetaObject>
#include <QMetaProperty>
#include <QString>
#include <QDomDocument>

#include "libmythupnp/httprequest.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class Xsd : public QDomDocument
{
    protected:

        static QString     ReadPropertyMetadata ( QObject *pObject, 
                                           const QString&  sPropName,
                                           const QString&  sKey );
                                         
        bool        RenderXSD            ( HTTPRequest *pRequest,
                                           QObject     *pClass );

        bool        RenderArrayXSD       ( HTTPRequest   *pRequest,
                                           const QString &sClassName,
                                           bool           bCustomType );

        bool        RenderMapXSD         ( HTTPRequest   *pRequest, 
                                           const QString &sClassName, 
                                           bool           bCustomType );


        QDomElement CreateSchemaRoot     ();

        QDomElement CreateComplexTypeNode( QMetaObject *pMetaObject );

        static bool        IsNillable           ( const QString       &sType );
        static bool        IsEnum               ( const QMetaProperty &metaProperty,
                                           const QString       &sType );

    public:

        bool GetXSD    ( HTTPRequest *pRequest, QString sTypeName );
        bool GetEnumXSD( HTTPRequest *pRequest, const QString& sEnumName );

        static QString ConvertTypeToXSD( const QString &sType, bool bCustomType = false );
};

//////////////////////////////////////////////////////////////////////////////

struct TypeInfo { QString sAttrName; QString sContentType; };

#endif // XSD_H

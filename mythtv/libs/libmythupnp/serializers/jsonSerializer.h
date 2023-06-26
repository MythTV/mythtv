//////////////////////////////////////////////////////////////////////////////
// Program Name: jsonSerializer.h
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Implementation for JSON
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef JSONSERIALIZER_H
#define JSONSERIALIZER_H

#include <QTextStream>
#include <QStringList>
#include <QVariantMap>

#include "upnpexp.h"
#include "serializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC JSONSerializer : public Serializer
{

    protected:

        QTextStream   m_stream;
        bool          m_bCommaNeeded {false};

        void BeginSerialize( QString &sName ) override; // Serializer
        void EndSerialize  () override; // Serializer

        void BeginObject( const QString &sName, const QObject  *pObject ) override; // Serializer
        void EndObject  ( const QString &sName, const QObject  *pObject ) override; // Serializer

        void AddProperty( const QString       &sName, 
                          const QVariant      &vValue,
                          const QMetaObject   *pMetaParent,
                          const QMetaProperty *pMetaProp ) override; // Serializer


        void RenderValue     ( const QVariant     &vValue );

        void RenderStringList( const QStringList  &list );
        void RenderList      ( const QVariantList &list );
        void RenderMap       ( const QVariantMap  &map  );

        static QString Encode       ( const QString &sIn );

    public:

                 JSONSerializer( QIODevice *pDevice,
                                 [[maybe_unused]] const QString &sRequestName )
                     : m_stream( pDevice ) {};
        virtual ~JSONSerializer() = default;

        QString GetContentType() override; // Serializer

};

#endif // JSONSERIALIZER_H

//////////////////////////////////////////////////////////////////////////////
// Program Name: jsonSerializer.h
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Implementation for JSON
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __JSONSERIALIZER_H__
#define __JSONSERIALIZER_H__

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

        QTextStream   m_Stream;
        bool          m_bCommaNeeded;

        virtual void BeginSerialize( QString &sName );
        virtual void EndSerialize  ();

        virtual void BeginObject( const QString &sName, const QObject  *pObject );
        virtual void EndObject  ( const QString &sName, const QObject  *pObject );

        virtual void AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp );


        void RenderValue     ( const QVariant     &vValue );

        void RenderStringList( const QStringList  &list );
        void RenderList      ( const QVariantList &list );
        void RenderMap       ( const QVariantMap  &map  );

        QString Encode       ( const QString &sIn );

    public:

                 JSONSerializer( QIODevice *pDevice, const QString &sRequestName );
        virtual ~JSONSerializer();

        virtual QString GetContentType();

};

#endif

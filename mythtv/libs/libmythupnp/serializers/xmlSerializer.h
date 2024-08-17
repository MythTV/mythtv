//////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.h
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Implementation for XML 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef XMLSERIALIZER_H
#define XMLSERIALIZER_H

#include <QXmlStreamWriter>
#include <QVariant>
#include <QIODevice>
#include <QStringList>

#include "upnpexp.h"
#include "serializer.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC XmlSerializer : public Serializer
{

    protected:

        QXmlStreamWriter *m_pXmlWriter   {nullptr};
        QString           m_sRequestName;
        bool              m_bIsRoot      {true};

        void BeginSerialize( QString &sName ) override; // Serializer
        void EndSerialize  () override; // Serializer

        void BeginObject( const QString &sName, const QObject  *pObject ) override; // Serializer
        void EndObject  ( const QString &sName, const QObject  *pObject ) override; // Serializer

        void AddProperty( const QString       &sName, 
                          const QVariant      &vValue,
                          const QMetaObject   *pMetaParent,
                          const QMetaProperty *pMetaProp ) override; // Serializer

        void    RenderValue     ( const QString &sName, const QVariant     &vValue );

        void    RenderEnum      ( const QString       &sName ,
                                  const QVariant      &vValue,
                                  const QMetaProperty *pMetaProp );


        void    RenderStringList( const QString &sName, const QStringList  &list );
        void    RenderList      ( const QString &sName, const QVariantList &list );
        void    RenderMap       ( const QString &sName, const QVariantMap  &map  );

        static QString GetItemName     ( const QString &sName );

        static QString GetContentName  ( const QString        &sName,
                                         const QMetaObject   *pMetaObject,
                                         const QMetaProperty *pMetaProp );

        static QString FindOptionValue ( const QStringList &sOptions, 
                                  const QString &sName );

    public:

        bool     PropertiesAsAttributes {true};

                 XmlSerializer( QIODevice *pDevice, QString sRequestName );
        virtual ~XmlSerializer();

        QString GetContentType() override; // Serializer

        // Deleted functions should be public.
        XmlSerializer(const XmlSerializer &) = delete;            // not copyable
        XmlSerializer &operator=(const XmlSerializer &) = delete; // not copyable
};

#endif // XMLSERIALIZER_H

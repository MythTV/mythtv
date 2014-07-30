//////////////////////////////////////////////////////////////////////////////
// Program Name: xmlSerializer.h
// Created     : Dec. 30, 2009
//
// Purpose     : Serialization Implementation for XML 
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __XMLSERIALIZER_H__
#define __XMLSERIALIZER_H__

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

        QXmlStreamWriter *m_pXmlWriter;
        QString           m_sRequestName;
        bool              m_bIsRoot;

        virtual void BeginSerialize( QString &sName );
        virtual void EndSerialize  ();

        virtual void BeginObject( const QString &sName, const QObject  *pObject );
        virtual void EndObject  ( const QString &sName, const QObject  *pObject );

        virtual void AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp );

        void    RenderValue     ( const QString &sName, const QVariant     &vValue );

        void    RenderEnum      ( const QString       &sName ,
                                  const QVariant      &vValue,
                                  const QMetaProperty *pMetaProp );


        void    RenderStringList( const QString &sName, const QStringList  &list );
        void    RenderList      ( const QString &sName, const QVariantList &list );
        void    RenderMap       ( const QString &sName, const QVariantMap  &map  );

        QString GetItemName     ( const QString &sName );

        QString GetContentName  ( const QString        &sName, 
                                  const QMetaObject   *pMetaObject,
                                  const QMetaProperty *pMetaProp );

        QString FindOptionValue ( const QStringList &sOptions, 
                                  const QString &sName );

    public:

        bool     PropertiesAsAttributes;

                 XmlSerializer( QIODevice *pDevice, const QString &sRequestName );
        virtual ~XmlSerializer();

        virtual QString GetContentType();

};

#endif

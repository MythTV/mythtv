//////////////////////////////////////////////////////////////////////////////
// Program Name: serializer.h
// Created     : Nov. 28, 2009
//
// Purpose     : Serialization Abstract Class
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "upnpexp.h"
#include "upnputil.h"

#include <QList>
#include <QMetaType>

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC Serializer
{
    protected:

        virtual void BeginSerialize( QString &sName ) {}
        virtual void EndSerialize  () {}

        virtual void BeginObject( const QString &sName, const QObject  *pObject ) = 0;
        virtual void EndObject  ( const QString &sName, const QObject  *pObject ) = 0;

        virtual void AddProperty( const QString       &sName, 
                                  const QVariant      &vValue,
                                  const QMetaObject   *pMetaParent,
                                  const QMetaProperty *pMetaProp ) = 0;

        //////////////////////////////////////////////////////////////////////

        void SerializeObject          ( const QObject *pObject, const QString &sName );
        void SerializeObjectProperties( const QObject *pObject );

    public:

        virtual void Serialize( const QObject *pObject, const QString &_sName = QString() );
        virtual void Serialize( const QVariant &vValue, const QString &sName );

        //////////////////////////////////////////////////////////////////////
        // Helper Methods
        //////////////////////////////////////////////////////////////////////

        virtual QString GetContentType () = 0;
        virtual void    AddHeaders     ( QStringMap &headers );


        Serializer()
        {
            qRegisterMetaType< QList<QObject*> >("QList<QObject*>");
        }
};

Q_DECLARE_METATYPE( QList<QObject*> )

#endif


//////////////////////////////////////////////////////////////////////////////
// Program Name: artworkInfoList.h
// Created     : Nov. 12, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ARTWORKINFOLIST_H_
#define ARTWORKINFOLIST_H_

#include <QString>
#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "artworkInfo.h"

namespace DTC
{

class SERVICE_PUBLIC ArtworkInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // We need to know the type that will ultimately be contained in
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "ArtworkInfos_type", "DTC::ArtworkInfo");

    Q_PROPERTY( QVariantList ArtworkInfos     READ ArtworkInfos DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, ArtworkInfos )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< ArtworkInfoList  >();
            qRegisterMetaType< ArtworkInfoList* >();

            ArtworkInfo::InitializeCustomTypes();
        }

    public:

        ArtworkInfoList(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        ArtworkInfoList( const ArtworkInfoList &src )
        {
            Copy( src );
        }

        void Copy( const ArtworkInfoList &src )
        {
            CopyListContents< ArtworkInfo >( this, m_ArtworkInfos, src.m_ArtworkInfos );
        }

        ArtworkInfo *AddNewArtworkInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            ArtworkInfo *pObject = new ArtworkInfo( this );
            m_ArtworkInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ArtworkInfoList  )
Q_DECLARE_METATYPE( DTC::ArtworkInfoList* )

#endif

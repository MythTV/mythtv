//////////////////////////////////////////////////////////////////////////////
// Program Name: musicMetadataInfoList.h
// Created     : July 20, 2017
//
// Copyright (c) 2017 Paul Harrison <pharrison@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MUSICMETADATAINFOLIST_H_
#define MUSICMETADATAINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "musicMetadataInfo.h"

namespace DTC
{

class SERVICE_PUBLIC MusicMetadataInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.00" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "MusicMetadataInfos", "type=DTC::MusicMetadataInfo");
    Q_CLASSINFO( "AsOf"              , "transient=true" );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          CurrentPage    READ CurrentPage     WRITE setCurrentPage    )
    Q_PROPERTY( int          TotalPages     READ TotalPages      WRITE setTotalPages     )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList MusicMetadataInfos READ MusicMetadataInfos )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , CurrentPage     )
    PROPERTYIMP       ( int         , TotalPages      )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP_REF   ( QDateTime   , AsOf            )
    PROPERTYIMP_REF   ( QString     , Version         )
    PROPERTYIMP_REF   ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, MusicMetadataInfos )

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE MusicMetadataInfoList(QObject *parent = nullptr)
            : QObject( parent ),
              m_StartIndex    ( 0      ),
              m_Count         ( 0      ),
              m_CurrentPage   ( 0      ),
              m_TotalPages    ( 0      ),
              m_TotalAvailable( 0      )
        {
        }

        void Copy( const MusicMetadataInfoList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< MusicMetadataInfo >( this, m_MusicMetadataInfos, src->m_MusicMetadataInfos );
        }

        MusicMetadataInfo *AddNewMusicMetadataInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new MusicMetadataInfo( this );
            m_MusicMetadataInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

inline void MusicMetadataInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< MusicMetadataInfoList* >();

    MusicMetadataInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif

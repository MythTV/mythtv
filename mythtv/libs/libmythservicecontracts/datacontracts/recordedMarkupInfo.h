//////////////////////////////////////////////////////////////////////////////
// Program Name: recordedMarkup.h
// Created     : Apr. 17, 2013
//
// Copyright (c) 2013 Tikinou LLC <dev-team@tikinou.com>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RECORDEDMARKUP_H_
#define RECORDEDMARKUP_H_

#include <QVariantList>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "recordedMarkupItem.h"

namespace DTC
{

class SERVICE_PUBLIC RecordedMarkupInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "Markups", "type=DTC::RecordedMarkupItem");

    Q_PROPERTY( int          ChannelId      READ ChannelId       WRITE setChannelId      )
    Q_PROPERTY( QDateTime    StartTime      READ StartTime       WRITE setStartTime      )
    Q_PROPERTY( int          DurationMs     READ DurationMs      WRITE setDurationMs     )
    Q_PROPERTY( int          VideoWidth     READ VideoWidth      WRITE setVideoWidth     )
    Q_PROPERTY( int          VideoHeight    READ VideoHeight     WRITE setVideoHeight    )
    Q_PROPERTY( int          VideoRate      READ VideoRate       WRITE setVideoRate      )
    Q_PROPERTY( int          TotalFrames    READ TotalFrames     WRITE setTotalFrames    )
    Q_PROPERTY( int          Bookmark       READ Bookmark        WRITE setBookmark       )

    Q_PROPERTY( QVariantList Markups READ Markups DESIGNABLE true )

    PROPERTYIMP       ( int         , ChannelId       )
    PROPERTYIMP       ( QDateTime   , StartTime       )
    PROPERTYIMP       ( int         , DurationMs      )
    PROPERTYIMP       ( int         , VideoWidth      )
    PROPERTYIMP       ( int         , VideoHeight     )
    PROPERTYIMP       ( int         , VideoRate       )
    PROPERTYIMP       ( int         , TotalFrames     )
    PROPERTYIMP       ( int         , Bookmark        )

    PROPERTYIMP_RO_REF( QVariantList, Markups )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< RecordedMarkupInfo   >();
            qRegisterMetaType< RecordedMarkupInfo*  >();

            RecordedMarkupItem::InitializeCustomTypes();
        }

    public:

        RecordedMarkupInfo(QObject *parent = 0)
            : QObject( parent ),
              m_DurationMs  ( 0 ),
              m_VideoWidth  ( 0 ),
              m_VideoHeight ( 0 ),
              m_VideoRate   ( 0 ),
              m_TotalFrames ( 0 ),
              m_Bookmark    ( 0 )
        {
        }

        RecordedMarkupInfo( const RecordedMarkupInfo &src )
        {
            Copy( src );
        }

        void Copy( const RecordedMarkupInfo &src )
        {
            m_ChannelId     = src.m_ChannelId      ;
            m_StartTime     = src.m_StartTime      ;
            m_DurationMs    = src.m_DurationMs     ;
            m_VideoWidth    = src.m_VideoWidth     ;
            m_VideoHeight   = src.m_VideoHeight    ;
            m_VideoRate     = src.m_VideoRate      ;
            m_TotalFrames   = src.m_TotalFrames    ;
            m_Bookmark      = src.m_Bookmark       ;

            CopyListContents< RecordedMarkupItem >( this, m_Markups, src.m_Markups );
        }

        RecordedMarkupItem *AddNewMarkupItem()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

        	RecordedMarkupItem *pObject = new RecordedMarkupItem( this );
            m_Markups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecordedMarkupInfo  )
Q_DECLARE_METATYPE( DTC::RecordedMarkupInfo* )

#endif

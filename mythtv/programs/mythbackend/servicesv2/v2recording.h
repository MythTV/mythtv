//////////////////////////////////////////////////////////////////////////////
// Program Name: recording.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2RECORDING_H_
#define V2RECORDING_H_

// Qt
#include <QDateTime>
#include <QString>

// MythTV
#include "libmythbase/http/mythhttpservice.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingstatus.h"

class V2RecordingInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.3" );

    SERVICE_PROPERTY2( uint                   , RecordedId  )
    SERVICE_PROPERTY2( int                    , Status      )
    SERVICE_PROPERTY2( RecStatus::Type        , StatusName  )
    SERVICE_PROPERTY2( QString                , RecTypeStatus  )
    SERVICE_PROPERTY2( int                    , Priority    )
    SERVICE_PROPERTY2( QDateTime              , StartTs     )
    SERVICE_PROPERTY2( QDateTime              , EndTs       )
    SERVICE_PROPERTY2( qlonglong              , FileSize    ) // v1.3
    SERVICE_PROPERTY2( QString                , FileName    ) // v1.3
    SERVICE_PROPERTY2( QString                , HostName    ) // v1.3
    SERVICE_PROPERTY2( QDateTime              , LastModified) // v1.3
    SERVICE_PROPERTY2( int                    , RecordId    )
    SERVICE_PROPERTY2( QString                , RecGroup    )
    SERVICE_PROPERTY2( QString                , StorageGroup)
    SERVICE_PROPERTY2( QString                , PlayGroup   )
    // This is actually enum type RecordingType
    SERVICE_PROPERTY2( int                    , RecType     )
    // This is actually enum type RecordingDupInType
    SERVICE_PROPERTY2( int                    , DupInType   )
    // This is actually enum type RecordingDupMethodType
    SERVICE_PROPERTY2( int                    , DupMethod   )
    SERVICE_PROPERTY2( int                    , EncoderId   )
    SERVICE_PROPERTY2( QString                , EncoderName )
    SERVICE_PROPERTY2( QString                , Profile     )

    public:

        Q_INVOKABLE V2RecordingInfo(QObject *parent = nullptr)
            : QObject           ( parent             ),
              m_Status          ( RecStatus::Unknown ),
              m_RecType         ( kNotRecording      ),
              m_DupInType       ( kDupsInRecorded    ),
              m_DupMethod       ( kDupCheckNone      )
        {
        }

        void Copy( const V2RecordingInfo *src )
        {
            m_RecordedId      = src->m_RecordedId       ;
            m_Status          = src->m_Status           ;
            m_StatusName      = src->m_StatusName       ;
            m_Priority        = src->m_Priority         ;
            m_StartTs         = src->m_StartTs          ;
            m_EndTs           = src->m_EndTs            ;
            m_FileSize        = src->m_FileSize         ;
            m_FileName        = src->m_FileName         ;
            m_HostName        = src->m_HostName         ;
            m_LastModified    = src->m_LastModified     ;
            m_RecordId        = src->m_RecordId         ;
            m_RecGroup        = src->m_RecGroup         ;
            m_StorageGroup    = src->m_StorageGroup     ;
            m_PlayGroup       = src->m_PlayGroup        ;
            m_RecType         = src->m_RecType          ;
            m_DupInType       = src->m_DupInType        ;
            m_DupMethod       = src->m_DupMethod        ;
            m_EncoderId       = src->m_EncoderId        ;
            m_EncoderName     = src->m_EncoderName      ;
            m_Profile         = src->m_Profile          ;
        }

    private:
        Q_DISABLE_COPY(V2RecordingInfo);
};

Q_DECLARE_METATYPE(V2RecordingInfo*)

#endif

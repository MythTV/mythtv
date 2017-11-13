//////////////////////////////////////////////////////////////////////////////
// Program Name: recording.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RECORDING_H_
#define RECORDING_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"
#include "enums/recStatus.h"
#include "programtypes.h"

namespace DTC
{

class SERVICE_PUBLIC RecordingInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.3" );

    Q_PROPERTY( uint             RecordedId   READ RecordedId   WRITE setRecordedId )
    Q_PROPERTY( RecStatus::Type  Status       READ Status       WRITE setStatus     )
    Q_PROPERTY( int              Priority     READ Priority     WRITE setPriority   )
    Q_PROPERTY( QDateTime        StartTs      READ StartTs      WRITE setStartTs    )
    Q_PROPERTY( QDateTime        EndTs        READ EndTs        WRITE setEndTs      )

    Q_PROPERTY( qlonglong        FileSize     READ FileSize     WRITE setFileSize     DESIGNABLE SerializeDetails ) // v1.3
    Q_PROPERTY( QString          FileName     READ FileName     WRITE setFileName     DESIGNABLE SerializeDetails ) // v1.3
    Q_PROPERTY( QString          HostName     READ HostName     WRITE setHostName     DESIGNABLE SerializeDetails ) // v1.3
    Q_PROPERTY( QDateTime        LastModified READ LastModified WRITE setLastModified DESIGNABLE SerializeDetails ) // v1.3

    Q_PROPERTY( int              RecordId     READ RecordId     WRITE setRecordId     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString          RecGroup     READ RecGroup     WRITE setRecGroup     DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString          PlayGroup    READ PlayGroup    WRITE setPlayGroup    DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString          StorageGroup READ StorageGroup WRITE setStorageGroup DESIGNABLE SerializeDetails )
    Q_PROPERTY( int              RecType      READ RecType      WRITE setRecType      DESIGNABLE SerializeDetails )
    Q_PROPERTY( int              DupInType    READ DupInType    WRITE setDupInType    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int              DupMethod    READ DupMethod    WRITE setDupMethod    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int              EncoderId    READ EncoderId    WRITE setEncoderId    DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString          EncoderName  READ EncoderName  WRITE setEncoderName  DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString          Profile      READ Profile      WRITE setProfile      DESIGNABLE SerializeDetails )

    PROPERTYIMP     ( uint                   , RecordedId  )
    PROPERTYIMP_ENUM( RecStatus::Type        , Status      )
    PROPERTYIMP     ( int                    , Priority    )
    PROPERTYIMP     ( QDateTime              , StartTs     )
    PROPERTYIMP     ( QDateTime              , EndTs       )

    PROPERTYIMP     ( qlonglong              , FileSize    ) // v1.3
    PROPERTYIMP     ( QString                , FileName    ) // v1.3
    PROPERTYIMP     ( QString                , HostName    ) // v1.3
    PROPERTYIMP     ( QDateTime              , LastModified) // v1.3

    PROPERTYIMP     ( int                    , RecordId    )
    PROPERTYIMP     ( QString                , RecGroup    )
    PROPERTYIMP     ( QString                , StorageGroup)
    PROPERTYIMP     ( QString                , PlayGroup   )
    PROPERTYIMP_ENUM( RecordingType          , RecType     )
    PROPERTYIMP_ENUM( RecordingDupInType     , DupInType   )
    PROPERTYIMP_ENUM( RecordingDupMethodType , DupMethod   )
    PROPERTYIMP     ( int                    , EncoderId   )
    PROPERTYIMP     ( QString                , EncoderName )
    PROPERTYIMP     ( QString                , Profile     )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails );

    public:

        RecordingInfo(QObject *parent = 0) 
            : QObject           ( parent             ),
              m_RecordedId      ( 0                  ),
              m_Status          ( RecStatus::Unknown ),
              m_Priority        ( 0                  ),
              m_FileSize        ( 0                  ),
              m_RecordId        ( 0                  ),
              m_RecType         ( kNotRecording      ),
              m_DupInType       ( kDupsInRecorded    ),
              m_DupMethod       ( kDupCheckNone      ),
              m_EncoderId       ( 0                  ),
              m_SerializeDetails( true               )
        {
        }

        void Copy( const RecordingInfo *src )
        {
            m_RecordedId      = src->m_RecordedId       ;
            m_Status          = src->m_Status           ;
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
            m_SerializeDetails= src->m_SerializeDetails ;
        }

    private:
        Q_DISABLE_COPY(RecordingInfo);
};

} // namespace DTC

#endif

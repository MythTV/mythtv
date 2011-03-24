//////////////////////////////////////////////////////////////////////////////
// Program Name: recording.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef RECORDING_H_
#define RECORDING_H_

#include <QDateTime>
#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"   
#include "programtypes.h"

namespace DTC
{

class SERVICE_PUBLIC RecordingInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_PROPERTY( int                     Status     READ Status     WRITE setStatus    )
    Q_PROPERTY( int                     Priority   READ Priority   WRITE setPriority  )
    Q_PROPERTY( QDateTime               StartTs    READ StartTs    WRITE setStartTs   )
    Q_PROPERTY( QDateTime               EndTs      READ EndTs      WRITE setEndTs     )
                                                   
    Q_PROPERTY( int                     RecordId   READ RecordId   WRITE setRecordId   DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString                 RecGroup   READ RecGroup   WRITE setRecGroup   DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString                 PlayGroup  READ PlayGroup  WRITE setPlayGroup  DESIGNABLE SerializeDetails )
    Q_PROPERTY( int                     RecType    READ RecType    WRITE setRecType    DESIGNABLE SerializeDetails )
    Q_PROPERTY( int                     DupInType  READ DupInType  WRITE setDupInType  DESIGNABLE SerializeDetails )
    Q_PROPERTY( int                     DupMethod  READ DupMethod  WRITE setDupMethod  DESIGNABLE SerializeDetails )
    Q_PROPERTY( int                     EncoderId  READ EncoderId  WRITE setEncoderId  DESIGNABLE SerializeDetails )
    Q_PROPERTY( QString                 Profile    READ Profile    WRITE setProfile    DESIGNABLE SerializeDetails )

    /*
    Not using since Q_ENUMS seem to require the enum be defined in this class

    Q_ENUMS( RecStatusType          )
    Q_ENUMS( RecordingType          )
    Q_ENUMS( RecordingDupInType     )
    Q_ENUMS( RecordingDupMethodType )
    */

    PROPERTYIMP_ENUM( RecStatusType          , Status      )
    PROPERTYIMP     ( int                    , Priority    )
    PROPERTYIMP     ( QDateTime              , StartTs     )
    PROPERTYIMP     ( QDateTime              , EndTs       )
                                                 
    PROPERTYIMP     ( int                    , RecordId    )
    PROPERTYIMP     ( QString                , RecGroup    )
    PROPERTYIMP     ( QString                , PlayGroup   )
    PROPERTYIMP_ENUM( RecordingType          , RecType     )
    PROPERTYIMP_ENUM( RecordingDupInType     , DupInType   )
    PROPERTYIMP_ENUM( RecordingDupMethodType , DupMethod   )
    PROPERTYIMP     ( int                    , EncoderId   )
    PROPERTYIMP     ( QString                , Profile     )

    // Used only by Serializer
    PROPERTYIMP( bool, SerializeDetails )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< RecordingInfo  >();
            qRegisterMetaType< RecordingInfo* >();
        }

    public:

        RecordingInfo(QObject *parent = 0) 
            : QObject           ( parent          ),
              m_Status          ( rsUnknown       ),
              m_Priority        ( 0               ),
              m_RecordId        ( 0               ),
              m_RecType         ( kNotRecording   ),
              m_DupInType       ( kDupsInRecorded ),
              m_DupMethod       ( kDupCheckNone   ),
              m_EncoderId       ( 0               ),
              m_SerializeDetails( true            ) 
        {
        }
        
        RecordingInfo( const RecordingInfo &src ) 
        {
            Copy( src );
        }

        void Copy( const RecordingInfo &src )
        {
            m_Status          = src.m_Status           ;
            m_Priority        = src.m_Priority         ;
            m_StartTs         = src.m_StartTs          ;
            m_EndTs           = src.m_EndTs            ;
            m_RecordId        = src.m_RecordId         ;
            m_RecGroup        = src.m_RecGroup         ;
            m_PlayGroup       = src.m_PlayGroup        ;
            m_RecType         = src.m_RecType          ;
            m_DupInType       = src.m_DupInType        ;
            m_DupMethod       = src.m_DupMethod        ;
            m_EncoderId       = src.m_EncoderId        ;
            m_Profile         = src.m_Profile          ;
            m_SerializeDetails= src.m_SerializeDetails ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecordingInfo  )
Q_DECLARE_METATYPE( DTC::RecordingInfo* )

#endif

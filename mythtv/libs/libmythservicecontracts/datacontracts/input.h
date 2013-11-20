//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef INPUT_H_
#define INPUT_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC Input : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( uint            Id              READ Id               WRITE setId             )
    Q_PROPERTY( uint            CardId          READ CardId           WRITE setCardId         )
    Q_PROPERTY( uint            SourceId        READ SourceId         WRITE setSourceId       )
    Q_PROPERTY( QString         InputName       READ InputName        WRITE setInputName      )
    Q_PROPERTY( QString         DisplayName     READ DisplayName      WRITE setDisplayName    )
//    Q_PROPERTY( QString         StartChan       READ StartChan        WRITE setStartChan      )
    Q_PROPERTY( bool            QuickTune       READ QuickTune        WRITE setQuickTune      )
    Q_PROPERTY( int             RecPriority     READ RecPriority      WRITE setRecPriority    )
    Q_PROPERTY( int             ScheduleOrder   READ ScheduleOrder    WRITE setScheduleOrder  )
    Q_PROPERTY( int             LiveTVOrder     READ LiveTVOrder      WRITE setLiveTVOrder    )

    PROPERTYIMP    ( uint       , Id             )
    PROPERTYIMP    ( uint       , CardId         )
    PROPERTYIMP    ( uint       , SourceId       )
    PROPERTYIMP    ( QString    , InputName      )
    PROPERTYIMP    ( QString    , DisplayName    )
//    PROPERTYIMP    ( QString       , StartChan      )
    PROPERTYIMP    ( bool       , QuickTune      )
    PROPERTYIMP    ( uint       , RecPriority    )
    PROPERTYIMP    ( uint       , ScheduleOrder  )
    PROPERTYIMP    ( uint       , LiveTVOrder    )

    public:

        static inline void InitializeCustomTypes();

    public:

        Input(QObject *parent = 0)
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_CardId        ( 0      ),
              m_SourceId      ( 0      ),
              m_QuickTune     ( false  ),
              m_RecPriority   ( 0      ),
              m_ScheduleOrder ( 0      ),
              m_LiveTVOrder   ( 0      )
        {
        }

        Input( const Input &src )
        {
            Copy( src );
        }

        void Copy( const Input &src )
        {
            m_Id            = src.m_Id;
            m_CardId        = src.m_CardId;
            m_SourceId      = src.m_SourceId;
            m_InputName     = src.m_InputName;
            m_DisplayName   = src.m_DisplayName;
//            m_StartChan     = src.m_StartChan;
            m_QuickTune     = src.m_QuickTune;
            m_RecPriority   = src.m_RecPriority;
            m_ScheduleOrder = src.m_ScheduleOrder;
            m_LiveTVOrder   = src.m_LiveTVOrder;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::Input  )
Q_DECLARE_METATYPE( DTC::Input* )

namespace DTC
{
inline void Input::InitializeCustomTypes()
{
    qRegisterMetaType< Input  >();
    qRegisterMetaType< Input* >();
}
}

#endif

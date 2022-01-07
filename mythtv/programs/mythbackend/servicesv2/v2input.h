//////////////////////////////////////////////////////////////////////////////
// Program Name: input.h
// Created     : Nov. 20, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2INPUT_H_
#define V2INPUT_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"

/////////////////////////////////////////////////////////////////////////////

class V2Input : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( uint       , Id             )
    SERVICE_PROPERTY2( uint       , CardId         )
    SERVICE_PROPERTY2( uint       , SourceId       )
    SERVICE_PROPERTY2( QString    , InputName      )
    SERVICE_PROPERTY2( QString    , DisplayName    )
    SERVICE_PROPERTY2( bool       , QuickTune      )
    SERVICE_PROPERTY2( int        , RecPriority    )
    SERVICE_PROPERTY2( int        , ScheduleOrder  )
    SERVICE_PROPERTY2( int        , LiveTVOrder    );

    public:

        Q_INVOKABLE V2Input(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2Input *src )
        {
            m_Id            = src->m_Id;
            m_CardId        = src->m_CardId;
            m_SourceId      = src->m_SourceId;
            m_InputName     = src->m_InputName;
            m_DisplayName   = src->m_DisplayName;
            m_QuickTune     = src->m_QuickTune;
            m_RecPriority   = src->m_RecPriority;
            m_ScheduleOrder = src->m_ScheduleOrder;
            m_LiveTVOrder   = src->m_LiveTVOrder;
        }

    private:
        Q_DISABLE_COPY(V2Input);
};

Q_DECLARE_METATYPE(V2Input*)

#endif

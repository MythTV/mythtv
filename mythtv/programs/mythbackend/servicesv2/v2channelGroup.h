//////////////////////////////////////////////////////////////////////////////
// Program Name: channelGroup.h
// Created     : Nov. 15, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CHANNELGROUP_H_
#define V2CHANNELGROUP_H_

#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

class  V2ChannelGroup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "0.99" );


    SERVICE_PROPERTY2( uint        , GroupId        )
    SERVICE_PROPERTY2( QString     , Name           )
    SERVICE_PROPERTY2( QString     , Password       );

    public:

        Q_INVOKABLE V2ChannelGroup(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2ChannelGroup *src )
        {
            m_GroupId      = src->m_GroupId     ;
            m_Name         = src->m_Name        ;
            m_Password     = src->m_Password    ;
        }

    private:
        Q_DISABLE_COPY(V2ChannelGroup);
};

Q_DECLARE_METATYPE(V2ChannelGroup*)

#endif

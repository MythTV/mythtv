//////////////////////////////////////////////////////////////////////////////
// Program Name: channelGroup.h
// Created     : Nov. 15, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2CHANNELRESTORE_H_
#define V2CHANNELRESTORE_H_

#include <QObject>
#include <QString>

#include "libmythbase/http/mythhttpservice.h"

class  V2ChannelRestore : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "0.99" );

    SERVICE_PROPERTY2( int         , NumChannels      )
    SERVICE_PROPERTY2( int         , NumXLMTVID       )
    SERVICE_PROPERTY2( int         , NumIcon          )
    SERVICE_PROPERTY2( int         , NumVisible       );

    public:

        Q_INVOKABLE V2ChannelRestore(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        // void Copy( const V2ChannelRestore *src )
        // {
        //     m_GroupId      = src->m_GroupId     ;
        //     m_Name         = src->m_Name        ;
        //     m_Password     = src->m_Password    ;
        // }

    private:
        Q_DISABLE_COPY(V2ChannelRestore);
};

Q_DECLARE_METATYPE(V2ChannelRestore*)

#endif

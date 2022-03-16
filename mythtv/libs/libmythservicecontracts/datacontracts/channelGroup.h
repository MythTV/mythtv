//////////////////////////////////////////////////////////////////////////////
// Program Name: channelGroup.h
// Created     : Nov. 15, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DTCCHANNELGROUP_H_
#define DTCCHANNELGROUP_H_

#include <QObject>
#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC ChannelGroup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "0.99" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_PROPERTY( uint      GroupId    READ GroupId   WRITE setGroupId   )
    Q_PROPERTY( QString   Name       READ Name      WRITE setName      )
    Q_PROPERTY( QString   Password   READ Password  WRITE setPassword  )

    PROPERTYIMP       ( uint        , GroupId        )
    PROPERTYIMP_REF   ( QString     , Name           )
    PROPERTYIMP_REF   ( QString     , Password       );

    public:

        static void InitializeCustomTypes();

        Q_INVOKABLE ChannelGroup(QObject *parent = nullptr)
            : QObject           ( parent ),
              m_GroupId         ( 0      )
        {
        }

        void Copy( const ChannelGroup *src )
        {
            m_GroupId      = src->m_GroupId     ;
            m_Name         = src->m_Name        ;
            m_Password     = src->m_Password    ;
        }

    private:
        Q_DISABLE_COPY(ChannelGroup);
};

inline void ChannelGroup::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelGroup*  >();
}

}

#endif

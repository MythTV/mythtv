//////////////////////////////////////////////////////////////////////////////
// Program Name: program.h
// Created     : Nov. 15, 2013
//
// Copyright (c) 2013 Stuart Morgan <smorgan@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef DTCCHANNELGROUP_H_
#define DTCCHANNELGROUP_H_

#include <QObject>
#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

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
    PROPERTYIMP       ( QString     , Name           )
    PROPERTYIMP       ( QString     , Password       )

    public:

        static void InitializeCustomTypes();

        ChannelGroup(QObject *parent = 0)
            : QObject           ( parent ),
              m_GroupId         ( 0      )
        {
        }

        ChannelGroup( const ChannelGroup &src )
        {
            Copy( src );
        }

        void Copy( const ChannelGroup &src )
        {
            m_GroupId      = src.m_GroupId     ;
            m_Name         = src.m_Name        ;
            m_Password     = src.m_Password    ;
        }

};

}

Q_DECLARE_METATYPE( DTC::ChannelGroup  )
Q_DECLARE_METATYPE( DTC::ChannelGroup* )

namespace DTC
{
inline void ChannelGroup::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelGroup   >();
    qRegisterMetaType< ChannelGroup*  >();
}
}

#endif

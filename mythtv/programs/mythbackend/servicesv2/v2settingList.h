//////////////////////////////////////////////////////////////////////////////
// Program Name: settingList.h
// Created     : Jan. 15, 2010
//
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2SETTINGLIST_H_
#define V2SETTINGLIST_H_

#include "libmythbase/http/mythhttpservice.h"

class V2SettingList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"      , "1.0" );

    Q_CLASSINFO( "Settings", "type=QString;name=String");

    SERVICE_PROPERTY2( QString,     HostName )
    Q_PROPERTY( QVariantMap Settings     READ Settings USER true )
    SERVICE_PROPERTY_RO_REF( QVariantMap, Settings )

    public:

        Q_INVOKABLE V2SettingList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2SettingList *src )
        {
            m_HostName = src->m_HostName;
            m_Settings = src->m_Settings;
        }

    private:
        Q_DISABLE_COPY(V2SettingList);
};

Q_DECLARE_METATYPE(V2SettingList*)

#endif

//////////////////////////////////////////////////////////////////////////////
// Program Name: settingList.h
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

#ifndef SETTINGLIST_H_
#define SETTINGLIST_H_

#include <QString>
#include <QVariantMap>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC SettingList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"      , "1.0" );

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "Settings_type", "QString");

    Q_PROPERTY( QString     HostName READ HostName WRITE setHostName )
    Q_PROPERTY( QVariantMap Settings READ Settings DESIGNABLE true )

    PROPERTYIMP       ( QString    , HostName )
    PROPERTYIMP_RO_REF( QVariantMap, Settings )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< SettingList  >();
            qRegisterMetaType< SettingList* >();
        }

    public:

        SettingList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        SettingList( const SettingList &src ) 
            : m_HostName( src.m_HostName ),
              m_Settings( src.m_Settings )    
        {
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::SettingList  )
Q_DECLARE_METATYPE( DTC::SettingList* )

#endif

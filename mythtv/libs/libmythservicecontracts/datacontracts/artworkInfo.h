//////////////////////////////////////////////////////////////////////////////
// Program Name: artworkInfo.h
// Created     : Nov. 12, 2011
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef ARTWORKINFO_H_
#define ARTWORKINFO_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC ArtworkInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString URL          READ URL          WRITE setURL          )
    Q_PROPERTY( QString FileName     READ FileName     WRITE setFileName     )
    Q_PROPERTY( QString StorageGroup READ StorageGroup WRITE setStorageGroup )
    Q_PROPERTY( QString Type         READ Type         WRITE setType         )

    PROPERTYIMP    ( QString    , URL            )
    PROPERTYIMP    ( QString    , FileName       )
    PROPERTYIMP    ( QString    , StorageGroup   )
    PROPERTYIMP    ( QString    , Type           )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< ArtworkInfo  >();
            qRegisterMetaType< ArtworkInfo* >();
        }

    public:

        ArtworkInfo(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        ArtworkInfo( const ArtworkInfo &src )
        {
            Copy( src );
        }

        void Copy( const ArtworkInfo &src )
        {
            m_URL           = src.m_URL           ;
            m_FileName      = src.m_FileName      ;
            m_StorageGroup  = src.m_StorageGroup  ;
            m_Type          = src.m_Type          ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ArtworkInfo  )
Q_DECLARE_METATYPE( DTC::ArtworkInfo* )

#endif

//////////////////////////////////////////////////////////////////////////////
// Program Name: timeZoneInfo.h
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

#ifndef TIMEZONEINFO_H_
#define TIMEZONEINFO_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC TimeZoneInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString    TimeZoneID      READ TimeZoneID
                                           WRITE setTimeZoneID      )
    Q_PROPERTY( int        UTCOffset       READ UTCOffset
                                           WRITE setUTCOffset       )
    Q_PROPERTY( QDateTime  CurrentDateTime READ CurrentDateTime
                                           WRITE setCurrentDateTime )

    PROPERTYIMP( QString  , TimeZoneID      )
    PROPERTYIMP( int      , UTCOffset       )
    PROPERTYIMP( QDateTime, CurrentDateTime )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< TimeZoneInfo  >();
            qRegisterMetaType< TimeZoneInfo* >();
        }

    public:

        TimeZoneInfo(QObject *parent = 0)
            : QObject             ( parent ),
              m_TimeZoneID        (        ),
              m_UTCOffset         ( 0      ),
              m_CurrentDateTime   (        )
        {
        }

        TimeZoneInfo( const TimeZoneInfo &src )
        {
            Copy( src );
        }

        void Copy( const TimeZoneInfo &src )
        {
            m_TimeZoneID      = src.m_TimeZoneID     ;
            m_UTCOffset       = src.m_UTCOffset      ;
            m_CurrentDateTime = src.m_CurrentDateTime;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::TimeZoneInfo )
Q_DECLARE_METATYPE( DTC::TimeZoneInfo* )

#endif

//////////////////////////////////////////////////////////////////////////////
// Program Name: timeZoneInfo.h
//
// Licensed under the GPL v2 or later, see COPYING for details
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

        static inline void InitializeCustomTypes();

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

namespace DTC
{
inline void TimeZoneInfo::InitializeCustomTypes()
{
    qRegisterMetaType< TimeZoneInfo  >();
    qRegisterMetaType< TimeZoneInfo* >();
}
}

#endif

#ifndef V2TIMEZONEINFO_H_
#define V2TIMEZONEINFO_H_

#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"
class V2TimeZoneInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString   , TimeZoneID      )
    SERVICE_PROPERTY2( int       , UTCOffset       )
    SERVICE_PROPERTY2( QDateTime , CurrentDateTime )

    public:

        // static inline void InitializeCustomTypes();

        Q_INVOKABLE V2TimeZoneInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2TimeZoneInfo *src )
        {
            m_TimeZoneID      = src->m_TimeZoneID     ;
            m_UTCOffset       = src->m_UTCOffset      ;
            m_CurrentDateTime = src->m_CurrentDateTime;
        }

    private:
        Q_DISABLE_COPY(V2TimeZoneInfo)
};

Q_DECLARE_METATYPE(V2TimeZoneInfo*)

#endif // V2TIMEZONEINFO_H_

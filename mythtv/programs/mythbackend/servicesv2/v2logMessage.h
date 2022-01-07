//////////////////////////////////////////////////////////////////////////////
// Program Name: logMessage.h
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2LOGMESSAGE_H_
#define V2LOGMESSAGE_H_

#include "libmythbase/http/mythhttpservice.h"

class V2LogMessage : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString  , HostName    )
    SERVICE_PROPERTY2( QString  , Application )
    SERVICE_PROPERTY2( int      , PID         )
    SERVICE_PROPERTY2( int      , TID         )
    SERVICE_PROPERTY2( QString  , Thread      )
    SERVICE_PROPERTY2( QString  , Filename    )
    SERVICE_PROPERTY2( int      , Line        )
    SERVICE_PROPERTY2( QString  , Function    )
    SERVICE_PROPERTY2( QDateTime, Time        )
    SERVICE_PROPERTY2( QString  , Level       )
    SERVICE_PROPERTY2( QString  , Message     );

    public:

        Q_INVOKABLE V2LogMessage(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LogMessage *src )
        {
            m_HostName        = src->m_HostName       ;
            m_Application     = src->m_Application    ;
            m_PID             = src->m_PID            ;
            m_TID             = src->m_TID            ;
            m_Thread          = src->m_Thread         ;
            m_Filename        = src->m_Filename       ;
            m_Line            = src->m_Line           ;
            m_Function        = src->m_Function       ;
            m_Time            = src->m_Time           ;
            m_Level           = src->m_Level          ;
            m_Message         = src->m_Message        ;
        }

    private:
        Q_DISABLE_COPY(V2LogMessage);
};

Q_DECLARE_METATYPE(V2LogMessage*)

#endif // V2LOGMESSAGE_H_

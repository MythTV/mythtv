#ifndef V2DATABASEINFO_H_
#define V2DATABASEINFO_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"

class V2DatabaseInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    SERVICE_PROPERTY2( QString , Host          )
    SERVICE_PROPERTY2( bool    , Ping          )
    SERVICE_PROPERTY2( int     , Port          )
    SERVICE_PROPERTY2( QString , UserName      )
    SERVICE_PROPERTY2( QString , Password      )
    SERVICE_PROPERTY2( QString , Name          )
    SERVICE_PROPERTY2( QString , Type          )
    SERVICE_PROPERTY2( bool    , LocalEnabled  )
    SERVICE_PROPERTY2( QString , LocalHostName )

    public:

        Q_INVOKABLE V2DatabaseInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2DatabaseInfo *src )
        {
            m_Host         = src->m_Host         ;
            m_Ping         = src->m_Ping         ;
            m_Port         = src->m_Port         ;
            m_UserName     = src->m_UserName     ;
            m_Password     = src->m_Password     ;
            m_Name         = src->m_Name         ;
            m_Type         = src->m_Type         ;
            m_LocalEnabled = src->m_LocalEnabled ;
            m_LocalHostName= src->m_LocalHostName;
        }

    private:
        Q_DISABLE_COPY(V2DatabaseInfo)
};

Q_DECLARE_METATYPE(V2DatabaseInfo*)

#endif // DATABASEINFO_H_

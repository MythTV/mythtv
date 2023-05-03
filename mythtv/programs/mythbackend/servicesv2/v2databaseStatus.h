#ifndef V2DATABASESTATUS_H_
#define V2DATABASESTATUS_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"

class V2DatabaseStatus : public QObject
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
    SERVICE_PROPERTY2( bool    , WOLEnabled    )
    SERVICE_PROPERTY2( int     , WOLReconnect  )
    SERVICE_PROPERTY2( int     , WOLRetry      )
    SERVICE_PROPERTY2( QString , WOLCommand    )
    SERVICE_PROPERTY2( bool    , Connected     )
    SERVICE_PROPERTY2( bool    , HaveDatabase  )
    SERVICE_PROPERTY2( int     , SchemaVersion )

    public:

        Q_INVOKABLE V2DatabaseStatus(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2DatabaseStatus)
};

Q_DECLARE_METATYPE(V2DatabaseStatus*)

#endif // DATABASESTATUS_H_

#ifndef V2CONNECTIONINFO_H_
#define V2CONNECTIONINFO_H_

#include "libmythbase/http/mythhttpservice.h"
#include "v2versionInfo.h"
#include "v2databaseInfo.h"
#include "v2wolInfo.h"

class V2ConnectionInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.1" );

    Q_PROPERTY( QObject*  Version  READ Version  USER true )
    Q_PROPERTY( QObject*  Database READ Database USER true )
    Q_PROPERTY( QObject*  WOL      READ WOL      USER true )

    SERVICE_PROPERTY_PTR( V2VersionInfo  , Version  )
    SERVICE_PROPERTY_PTR( V2DatabaseInfo , Database )
    SERVICE_PROPERTY_PTR( V2WOLInfo      , WOL      )

  public:

    Q_INVOKABLE V2ConnectionInfo(QObject *parent = nullptr)
        : QObject( parent )
    {
    }

};

Q_DECLARE_METATYPE(V2ConnectionInfo*)

#endif // V2CONNECTIONINFO_H_

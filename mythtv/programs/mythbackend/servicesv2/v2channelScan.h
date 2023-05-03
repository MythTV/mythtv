#ifndef V2CHANNELSCAN_H_
#define V2CHANNELSCAN_H_

#include "libmythbase/http/mythhttpservice.h"

class  V2ScanStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    SERVICE_PROPERTY2 ( int         , CardId        )
    SERVICE_PROPERTY2 ( QString     , Status        )
    SERVICE_PROPERTY2 ( bool        , SignalLock    )
    SERVICE_PROPERTY2 ( int         , Progress      )
    SERVICE_PROPERTY2 ( int         , SignalNoise   )
    SERVICE_PROPERTY2 ( int         , SignalStrength)
    SERVICE_PROPERTY2 ( QString     , StatusLog     )
    SERVICE_PROPERTY2 ( QString     , StatusText    )
    SERVICE_PROPERTY2 ( QString     , StatusTitle   )
    SERVICE_PROPERTY2 ( QString     , DialogMsg     )
    SERVICE_PROPERTY2 ( bool        , DialogInputReq )
    SERVICE_PROPERTY2 ( QStringList , DialogButtons )


    public:

        Q_INVOKABLE V2ScanStatus(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2ScanStatus);
};

Q_DECLARE_METATYPE(V2ScanStatus*)

class  V2Scan : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    SERVICE_PROPERTY2 ( int         , ScanId        )
    SERVICE_PROPERTY2 ( int         , CardId        )
    SERVICE_PROPERTY2 ( int         , SourceId      )
    SERVICE_PROPERTY2 ( bool        , Processed     )
    SERVICE_PROPERTY2 ( QDateTime   , ScanDate      )

    public:

        Q_INVOKABLE V2Scan(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2Scan);
};

Q_DECLARE_METATYPE(V2Scan*)

class  V2ScanList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "Scans", "type=V2Scan");

    SERVICE_PROPERTY2 ( QVariantList  , Scans       )

    public:

        Q_INVOKABLE V2ScanList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        V2Scan *AddNewScan()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Scan( this );
            m_Scans.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }


    private:
        Q_DISABLE_COPY(V2ScanList);
};

Q_DECLARE_METATYPE(V2ScanList*)

#endif // V2CHANNELSCAN_H_

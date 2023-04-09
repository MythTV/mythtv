#ifndef V2SYSTEMEVENTLIST_H_
#define V2SYSTEMEVENTLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

class V2SystemEvent : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString     , Key            )
    SERVICE_PROPERTY2( QString     , LocalizedName  )
    SERVICE_PROPERTY2( QString     , Value  )

  public:
    Q_INVOKABLE V2SystemEvent(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2SystemEvent)
};

Q_DECLARE_METATYPE(V2SystemEvent*)


class V2SystemEventList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_CLASSINFO( "SystemEvents", "type=V2SystemEvent");

    SERVICE_PROPERTY2( QVariantList, SystemEvents );

    public:

        Q_INVOKABLE V2SystemEventList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        V2SystemEvent *AddNewSystemEvent()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2SystemEvent( this );
            m_SystemEvents.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2SystemEventList);
};

Q_DECLARE_METATYPE(V2SystemEventList*)

#endif //V2SYSTEMEVENTLIST_H_

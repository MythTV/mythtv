#ifndef V2VIDEOMULTIPLEXLIST_H_
#define V2VIDEOMULTIPLEXLIST_H_

#include <QVariantList>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2videoMultiplex.h"

class V2VideoMultiplexList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "VideoMultiplexes", "type=V2VideoMultiplex");
    Q_CLASSINFO( "AsOf"            , "transient=true"          );

    SERVICE_PROPERTY2( int         , StartIndex      )
    SERVICE_PROPERTY2( int         , Count           )
    SERVICE_PROPERTY2( int         , CurrentPage     )
    SERVICE_PROPERTY2( int         , TotalPages      )
    SERVICE_PROPERTY2( int         , TotalAvailable  )
    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, VideoMultiplexes );

    public:

        Q_INVOKABLE V2VideoMultiplexList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoMultiplexList *src )
        {
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2VideoMultiplex >( this, m_VideoMultiplexes, src->m_VideoMultiplexes );
        }

        V2VideoMultiplex *AddNewVideoMultiplex()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoMultiplex( this );
            m_VideoMultiplexes.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoMultiplexList);
};

Q_DECLARE_METATYPE(V2VideoMultiplexList*)

#endif

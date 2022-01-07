#ifndef V2VIDEOSOURCELIST_H_
#define V2VIDEOSOURCELIST_H_

#include <QVariantList>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2videoSource.h"


class V2VideoSourceList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "VideoSources", "type=V2VideoSource");
    Q_CLASSINFO( "AsOf"        , "transient=true"       );


    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, VideoSources )

    public:

        Q_INVOKABLE V2VideoSourceList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2VideoSourceList *src )
        {
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2VideoSource >( this, m_VideoSources, src->m_VideoSources );
        }

        V2VideoSource *AddNewVideoSource()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2VideoSource( this );
            m_VideoSources.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2VideoSourceList);
};

Q_DECLARE_METATYPE(V2VideoSourceList*)

#endif

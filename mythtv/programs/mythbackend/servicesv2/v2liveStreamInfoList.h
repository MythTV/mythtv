#ifndef V2LIVESTREAMINFOLIST_H_
#define V2LIVESTREAMINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2liveStreamInfo.h"

class V2LiveStreamInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );
    Q_CLASSINFO( "LiveStreamInfos", "type=V2LiveStreamInfo");

    SERVICE_PROPERTY2( QVariantList, LiveStreamInfos );

    public:

        explicit V2LiveStreamInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LiveStreamInfoList *src )
        {
            CopyListContents< V2LiveStreamInfo >( this, m_LiveStreamInfos, src->m_LiveStreamInfos );
        }

        V2LiveStreamInfo *AddNewLiveStreamInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2LiveStreamInfo( this );
            m_LiveStreamInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2LiveStreamInfoList);
};

Q_DECLARE_METATYPE(V2LiveStreamInfoList*)

#endif

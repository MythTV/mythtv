#ifndef V2CHANNELINFOLIST_H_
#define V2CHANNELINFOLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"

#include "v2programAndChannel.h"

class  V2ChannelInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "ChannelInfos", "type=V2ChannelInfo");
    Q_CLASSINFO( "AsOf"        , "transient=true"       );

    SERVICE_PROPERTY2 ( int         , StartIndex      )
    SERVICE_PROPERTY2 ( int         , Count           )
    SERVICE_PROPERTY2 ( int         , CurrentPage     )
    SERVICE_PROPERTY2 ( int         , TotalPages      )
    SERVICE_PROPERTY2 ( int         , TotalAvailable  )
    SERVICE_PROPERTY2 ( QDateTime   , AsOf            )
    SERVICE_PROPERTY2 ( QString     , Version         )
    SERVICE_PROPERTY2 ( QString     , ProtoVer        )
    SERVICE_PROPERTY2 ( QVariantList, ChannelInfos );

    public:

        Q_INVOKABLE V2ChannelInfoList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2ChannelInfoList *src )
        {
            m_StartIndex    = src->m_StartIndex     ;
            m_Count         = src->m_Count          ;
            m_TotalAvailable= src->m_TotalAvailable ;
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2ChannelInfo >( this, m_ChannelInfos, src->m_ChannelInfos );
        }

        V2ChannelInfo *AddNewChannelInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2ChannelInfo( this );
            m_ChannelInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2ChannelInfoList);
};

Q_DECLARE_METATYPE(V2ChannelInfoList*)

#endif

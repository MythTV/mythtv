#ifndef VIDEOSOURCELIST_H_
#define VIDEOSOURCELIST_H_

#include <QVariantList>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "videoSource.h"

namespace DTC
{

class SERVICE_PUBLIC VideoSourceList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "VideoSources_type", "DTC::VideoSource");

    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList VideoSources READ VideoSources DESIGNABLE true )

    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, VideoSources )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< VideoSourceList   >();
            qRegisterMetaType< VideoSourceList*  >();

            VideoSource::InitializeCustomTypes();
        }

    public:

        VideoSourceList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        VideoSourceList( const VideoSourceList &src )
        {
            Copy( src );
        }

        void Copy( const VideoSourceList &src )
        {
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< VideoSource >( this, m_VideoSources, src.m_VideoSources );
        }

        VideoSource *AddNewVideoSource()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            VideoSource *pObject = new VideoSource( this );
            m_VideoSources.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::VideoSourceList  )
Q_DECLARE_METATYPE( DTC::VideoSourceList* )

#endif

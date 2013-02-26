#ifndef LIVESTREAMINFOLIST_H_
#define LIVESTREAMINFOLIST_H_

#include <QVariantList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "liveStreamInfo.h"

namespace DTC
{

class SERVICE_PUBLIC LiveStreamInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "LiveStreamInfos", "type=DTC::LiveStreamInfo");

    Q_PROPERTY( QVariantList LiveStreamInfos READ LiveStreamInfos DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, LiveStreamInfos )

    public:

        static inline void InitializeCustomTypes();

    public:

        LiveStreamInfoList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        LiveStreamInfoList( const LiveStreamInfoList &src ) 
        {
            Copy( src );
        }

        void Copy( const LiveStreamInfoList &src )
        {
            CopyListContents< LiveStreamInfo >( this, m_LiveStreamInfos, src.m_LiveStreamInfos );
        }

        LiveStreamInfo *AddNewLiveStreamInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            LiveStreamInfo *pObject = new LiveStreamInfo( this );
            m_LiveStreamInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LiveStreamInfoList  )
Q_DECLARE_METATYPE( DTC::LiveStreamInfoList* )

namespace DTC
{
inline void LiveStreamInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< LiveStreamInfoList   >();
    qRegisterMetaType< LiveStreamInfoList*  >();

    LiveStreamInfo::InitializeCustomTypes();
}
}

#endif

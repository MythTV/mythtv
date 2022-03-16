#ifndef LIVESTREAMINFOLIST_H_
#define LIVESTREAMINFOLIST_H_

#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

#include "libmythservicecontracts/datacontracts/liveStreamInfo.h"

namespace DTC
{

class SERVICE_PUBLIC LiveStreamInfoList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "LiveStreamInfos", "type=DTC::LiveStreamInfo");

    Q_PROPERTY( QVariantList LiveStreamInfos READ LiveStreamInfos )

    PROPERTYIMP_RO_REF( QVariantList, LiveStreamInfos );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit LiveStreamInfoList(QObject *parent = nullptr)
            : QObject( parent )               
        {
        }

        void Copy( const LiveStreamInfoList *src )
        {
            CopyListContents< LiveStreamInfo >( this, m_LiveStreamInfos, src->m_LiveStreamInfos );
        }

        LiveStreamInfo *AddNewLiveStreamInfo()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new LiveStreamInfo( this );
            m_LiveStreamInfos.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(LiveStreamInfoList);
};

inline void LiveStreamInfoList::InitializeCustomTypes()
{
    qRegisterMetaType< LiveStreamInfoList*  >();

    LiveStreamInfo::InitializeCustomTypes();
}

} // namespace DTC

#endif

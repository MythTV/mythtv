#ifndef CHANNELGROUPLIST_H_
#define CHANNELGROUPLIST_H_

#include <QVariantList>
#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "channelGroup.h"

namespace DTC
{

class SERVICE_PUBLIC ChannelGroupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "ChannelGroups", "type=DTC::ChannelGroup");

    Q_PROPERTY( QVariantList ChannelGroups READ ChannelGroups DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, ChannelGroups )

    public:

        static inline void InitializeCustomTypes();

        ChannelGroupList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        ChannelGroupList( const ChannelGroupList &src )
        {
            Copy( src );
        }

        void Copy( const ChannelGroupList &src )
        {
            CopyListContents< ChannelGroup >( this, m_ChannelGroups, src.m_ChannelGroups );
        }

        ChannelGroup *AddNewChannelGroup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            ChannelGroup *pObject = new ChannelGroup( this );
            m_ChannelGroups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::ChannelGroupList  )
Q_DECLARE_METATYPE( DTC::ChannelGroupList* )

namespace DTC
{
inline void ChannelGroupList::InitializeCustomTypes()
{
    qRegisterMetaType< ChannelGroupList   >();
    qRegisterMetaType< ChannelGroupList*  >();

    ChannelGroup::InitializeCustomTypes();
}
}

#endif

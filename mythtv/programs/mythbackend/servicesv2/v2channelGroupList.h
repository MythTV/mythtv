#ifndef V2CHANNELGROUPLIST_H_
#define V2CHANNELGROUPLIST_H_

#include <QVariantList>
#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2channelGroup.h"


class  V2ChannelGroupList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "ChannelGroups", "type=V2ChannelGroup");

    SERVICE_PROPERTY2( QVariantList, ChannelGroups );

    public:

        Q_INVOKABLE V2ChannelGroupList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2ChannelGroupList *src )
        {
            CopyListContents< V2ChannelGroup >( this, m_ChannelGroups, src->m_ChannelGroups );
        }

        V2ChannelGroup *AddNewChannelGroup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2ChannelGroup( this );
            m_ChannelGroups.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2ChannelGroupList);
};

Q_DECLARE_METATYPE(V2ChannelGroupList*)

#endif

#ifndef V2CASTMEMBERLIST_H_
#define V2CASTMEMBERLIST_H_

#include <QVariantList>
#include <QString>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2castMember.h"


class V2CastMemberList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "0.99" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See mythhttpservice.h for details

    Q_CLASSINFO( "CastMembers", "type=V2CastMember");

    SERVICE_PROPERTY2( QVariantList, CastMembers );

    public:

        Q_INVOKABLE V2CastMemberList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CastMemberList *src )
        {
            CopyListContents< V2CastMember >( this, m_CastMembers, src->m_CastMembers );
        }

        V2CastMember *AddNewCastMember()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2CastMember( this );
            m_CastMembers.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CastMemberList);
};

Q_DECLARE_METATYPE(V2CastMemberList*)

#endif

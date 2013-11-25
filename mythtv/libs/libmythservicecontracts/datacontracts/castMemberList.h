#ifndef CASTMEMBERLIST_H_
#define CASTMEMBERLIST_H_

#include <QVariantList>
#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "castMember.h"

namespace DTC
{

class SERVICE_PUBLIC CastMemberList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "0.99" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "CastMembers", "type=DTC::CastMember");

    Q_PROPERTY( QVariantList CastMembers READ CastMembers DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, CastMembers )

    public:

        static inline void InitializeCustomTypes();

        CastMemberList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        CastMemberList( const CastMemberList &src )
        {
            Copy( src );
        }

        void Copy( const CastMemberList &src )
        {
            CopyListContents< CastMember >( this, m_CastMembers, src.m_CastMembers );
        }

        CastMember *AddNewCastMember()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            CastMember *pObject = new CastMember( this );
            m_CastMembers.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::CastMemberList  )
Q_DECLARE_METATYPE( DTC::CastMemberList* )

namespace DTC
{
inline void CastMemberList::InitializeCustomTypes()
{
    qRegisterMetaType< CastMemberList   >();
    qRegisterMetaType< CastMemberList*  >();

    CastMember::InitializeCustomTypes();
}
}

#endif

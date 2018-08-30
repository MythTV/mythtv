#ifndef KEYBINDLIST_H_
#define KEYBINDLIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "keyBind.h"

namespace DTC
{

class SERVICE_PUBLIC KeyBindList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "KeyBinds", "type=DTC::KeyBind");

    Q_PROPERTY( QVariantList KeyBinds READ KeyBinds DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, KeyBinds )

    public:

        static inline void InitializeCustomTypes();

    public:

        KeyBindList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        KeyBindList( const KeyBindList &src )
        {
            Copy( src );
        }

        void Copy( const KeyBindList &src )
        {
            CopyListContents< KeyBind >( this, m_KeyBinds, src.m_KeyBinds );
        }

        KeyBind *AddNewKeyBind()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            KeyBind *pObject = new KeyBind( this );
            m_KeyBinds.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::KeyBindList  )
Q_DECLARE_METATYPE( DTC::KeyBindList* )

namespace DTC
{
inline void KeyBindList::InitializeCustomTypes()
{
    qRegisterMetaType< KeyBindList   >();
    qRegisterMetaType< KeyBindList*  >();

    KeyBind::InitializeCustomTypes();
}
}

#endif

#ifndef STORAGEGROUPDIRLIST_H_
#define STORAGEGROUPDIRLIST_H_

#include <QVariantList>

#include "serviceexp.h" 
#include "datacontracthelper.h"

#include "storageGroupDir.h"

namespace DTC
{

class SERVICE_PUBLIC StorageGroupDirList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "StorageGroupDirs_type", "DTC::StorageGroupDir");

    Q_PROPERTY( QVariantList StorageGroupDirs READ StorageGroupDirs DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, StorageGroupDirs )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< StorageGroupDirList   >();
            qRegisterMetaType< StorageGroupDirList*  >();

            StorageGroupDir::InitializeCustomTypes();
        }

    public:

        StorageGroupDirList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }
        
        StorageGroupDirList( const StorageGroupDirList &src ) 
        {
            Copy( src );
        }

        void Copy( const StorageGroupDirList &src )
        {
            CopyListContents< StorageGroupDir >( this, m_StorageGroupDirs, src.m_StorageGroupDirs );
        }

        StorageGroupDir *AddNewStorageGroupDir()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            StorageGroupDir *pObject = new StorageGroupDir( this );
            m_StorageGroupDirs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::StorageGroupDirList  )
Q_DECLARE_METATYPE( DTC::StorageGroupDirList* )

#endif

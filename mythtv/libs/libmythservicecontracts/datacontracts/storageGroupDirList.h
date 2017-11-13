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

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "StorageGroupDirs", "type=DTC::StorageGroupDir");

    Q_PROPERTY( QVariantList StorageGroupDirs READ StorageGroupDirs DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, StorageGroupDirs );

    public:

        StorageGroupDirList(QObject *parent = 0) 
            : QObject( parent )               
        {
        }

        void Copy( const StorageGroupDirList *src )
        {
            CopyListContents< StorageGroupDir >( this, m_StorageGroupDirs, src->m_StorageGroupDirs );
        }

        StorageGroupDir *AddNewStorageGroupDir()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            StorageGroupDir *pObject = new StorageGroupDir( this );
            m_StorageGroupDirs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(StorageGroupDirList);
};

} // namespace DTC

#endif

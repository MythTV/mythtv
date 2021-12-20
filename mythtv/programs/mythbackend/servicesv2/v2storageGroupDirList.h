#ifndef V2STORAGEGROUPDIRLIST_H_
#define V2STORAGEGROUPDIRLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2storageGroupDir.h"

class V2StorageGroupDirList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_CLASSINFO( "StorageGroupDirs", "type=V2StorageGroupDir");

    SERVICE_PROPERTY2( QVariantList, StorageGroupDirs );

    public:

        Q_INVOKABLE V2StorageGroupDirList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2StorageGroupDirList *src )
        {
            CopyListContents< V2StorageGroupDir >( this, m_StorageGroupDirs, src->m_StorageGroupDirs );
        }

        V2StorageGroupDir *AddNewStorageGroupDir()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2StorageGroupDir( this );
            m_StorageGroupDirs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2StorageGroupDirList);
};

Q_DECLARE_METATYPE(V2StorageGroupDirList*)

#endif

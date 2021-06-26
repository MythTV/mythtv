#ifndef V2STORAGEGROUPDIRLIST_H_
#define V2STORAGEGROUPDIRLIST_H_

#include <QVariantList>
#include "libmythbase/http/mythhttpservice.h"
#include "v2storageGroupDir.h"
#if SGDL
class V2StorageGroupDirList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );
    Q_CLASSINFO( "StorageGroupDirs", "type=V2StorageGroupDir");

    Q_PROPERTY( QObject* ServiceGroupDir READ ServiceGroupDir USER true)

    SERVICE_PROPERTY2   (QVariantList, StorageGroupDir)

    public:

        V2StorageGroupDirList(QObject *parent = nullptr)
                              : QObject( parent ),
                              m_StorageGroupDir ( nullptr )
        {
        }

        void Copy( const V2StorageGroupDir *src )
        {
            if ( src->mStorageGroupDir != nullprt )
                StorageGroupDir()->Copy( src->m_StorageGroupDir );
        }

        V2StorageGroupDir *AddNewStorageGroupDir()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2StorageGroupDir( this );
            m_StorageGroupDirs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

Q_DECLARE_METATYPE(V2StorageGroupDirList*)
#endif // SGDL
#endif // V2STORAGEGROUPDIRLIST_H_

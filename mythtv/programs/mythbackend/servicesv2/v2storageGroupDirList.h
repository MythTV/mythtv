#ifndef V2STORAGEGROUPDIRLIST_H_
#define V2STORAGEGROUPDIRLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2storageGroupDir.h"

class V2StorageGroupDirList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( QString , GroupName )
    SERVICE_PROPERTY2( QString , HostName  )

    public:

        V2StorageGroupDirList(QObject *parent = nullptr)
                              : QObject( parent )
        {
        }

/*
        void Copy( const V2StorageGroupDirList *src )
        {
            m_GroupName = src->m_GroupName     ;
            m_HostName  = src->m_HostName      ;
        }

        V2StorageGroupDir *AddNewStorageGroupDir()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2StorageGroupDir( this );
            V2StorageGroupDir.append( QVariant::fromValue<QObject *>( pObject ));
            // WAS: m_StorageGroupDirs.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }
*/
    private:
        Q_DISABLE_COPY(V2StorageGroupDirList)
};

Q_DECLARE_METATYPE(V2StorageGroupDirList*)

#endif // V2STORAGEGROUPDIRLIST_H_

#ifndef V2STORAGEGROUPDIR_H_
#define V2STORAGEGROUPDIR_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"
class V2StorageGroupDir : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.1" );

    SERVICE_PROPERTY2( int         , Id             )
    SERVICE_PROPERTY2( QString     , GroupName      )
    SERVICE_PROPERTY2( QString     , HostName       )
    SERVICE_PROPERTY2( QString     , DirName        )
    SERVICE_PROPERTY2( bool        , DirRead        )
    SERVICE_PROPERTY2( bool        , DirWrite       )
    SERVICE_PROPERTY2( uint        , KiBFree        )

  public:
    Q_INVOKABLE V2StorageGroupDir(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    void Copy( const V2StorageGroupDir *src )
    {
        m_Id        = src->m_Id            ;
        m_GroupName = src->m_GroupName     ;
        m_HostName  = src->m_HostName      ;
        m_DirName   = src->m_DirName       ;
        m_DirRead   = src->m_DirRead       ;
        m_DirWrite  = src->m_DirWrite      ;
        m_KiBFree   = src->m_KiBFree       ;
    }

    private:
        Q_DISABLE_COPY(V2StorageGroupDir)
};

Q_DECLARE_METATYPE(V2StorageGroupDir*)

#endif // V2STORAGEGROUPDIR_H_

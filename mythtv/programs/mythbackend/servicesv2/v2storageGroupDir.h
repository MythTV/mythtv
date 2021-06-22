#ifndef V2STORAGEGROUPDIR_H_
#define V2STORAGEGROUPDIR_H_

#include <QString>

#include "libmythbase/http/mythhttpservice.h"
//////////////// #include "serviceexp.h"
//////////////// #include "datacontracthelper.h"

class V2StorageGroupDir : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.1" );

    SERVICE_PROPERTY2( int         , Id             )
    SERVICE_PROPERTY2( QString     , GroupName      )
    SERVICE_PROPERTY2( QString     , HostName       )
    SERVICE_PROPERTY2( QString     , DirName        )
    SERVICE_PROPERTY2( bool        , DirRead        )
    SERVICE_PROPERTY2( bool        , DirWrite       )
    SERVICE_PROPERTY2( uint        , KiBFree        )

  public:
    V2StorageGroupDir(QObject *parent = nullptr)
                      : QObject         ( parent ),
                      m_Id            ( 0      ),
                      m_DirRead       ( false  ),
                      m_DirWrite      ( false  ),
                      m_KiBFree       ( 0      )

        {
        }
    private:
        Q_DISABLE_COPY(V2StorageGroupDir)
};

Q_DECLARE_METATYPE(V2StorageGroupDir*)

#endif // V2STORAGEGROUPDIR_H_

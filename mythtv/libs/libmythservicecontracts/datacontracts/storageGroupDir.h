#ifndef STORAGEGROUPDIR_H_
#define STORAGEGROUPDIR_H_

#include <QString>

#include "serviceexp.h" 
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC StorageGroupDir : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         GroupName       READ GroupName        WRITE setGroupName      )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( QString         DirName         READ DirName          WRITE setDirName        )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP    ( QString    , GroupName      )
    PROPERTYIMP    ( QString    , HostName       )
    PROPERTYIMP    ( QString    , DirName        )

    public:

        static inline void InitializeCustomTypes();

    public:

        StorageGroupDir(QObject *parent = 0) 
            : QObject         ( parent ),
              m_Id            ( 0      )
        { 
        }
        
        StorageGroupDir( const StorageGroupDir &src )
        {
            Copy( src );
        }

        void Copy( const StorageGroupDir &src )
        {
            m_Id            = src.m_Id            ;
            m_GroupName     = src.m_GroupName     ;
            m_HostName      = src.m_HostName      ;
            m_DirName       = src.m_DirName       ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::StorageGroupDir  )
Q_DECLARE_METATYPE( DTC::StorageGroupDir* )

namespace DTC
{
inline void StorageGroupDir::InitializeCustomTypes()
{
    qRegisterMetaType< StorageGroupDir   >();
    qRegisterMetaType< StorageGroupDir*  >();
}
}

#endif

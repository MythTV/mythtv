#ifndef STORAGEGROUPDIR_H_
#define STORAGEGROUPDIR_H_

#include <QString>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC StorageGroupDir : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.1" );

    Q_PROPERTY( int             Id              READ Id               WRITE setId             )
    Q_PROPERTY( QString         GroupName       READ GroupName        WRITE setGroupName      )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( QString         DirName         READ DirName          WRITE setDirName        )
    Q_PROPERTY( bool            DirRead         READ DirRead          WRITE setDirRead        )
    Q_PROPERTY( bool            DirWrite        READ DirWrite         WRITE setDirWrite       )
    Q_PROPERTY( uint            KiBFree         READ KiBFree          WRITE setKiBFree        )

    PROPERTYIMP    ( int        , Id             )
    PROPERTYIMP_REF( QString    , GroupName      )
    PROPERTYIMP_REF( QString    , HostName       )
    PROPERTYIMP_REF( QString    , DirName        )
    PROPERTYIMP    ( bool       , DirRead        )
    PROPERTYIMP    ( bool       , DirWrite       )
    PROPERTYIMP    ( uint       , KiBFree        );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE StorageGroupDir(QObject *parent = nullptr)
            : QObject         ( parent ),
              m_Id            ( 0      ),
              m_DirRead       ( false  ),
              m_DirWrite      ( false  ),
              m_KiBFree       ( 0      )
        {
        }

        void Copy( const StorageGroupDir *src )
        {
            m_Id            = src->m_Id            ;
            m_GroupName     = src->m_GroupName     ;
            m_HostName      = src->m_HostName      ;
            m_DirName       = src->m_DirName       ;
            m_DirRead       = src->m_DirRead       ;
            m_DirWrite      = src->m_DirWrite      ;
            m_KiBFree       = src->m_KiBFree       ;
        }

    private:
        Q_DISABLE_COPY(StorageGroupDir);
};

inline void StorageGroupDir::InitializeCustomTypes()
{
    qRegisterMetaType< StorageGroupDir*  >();
}

} // namespace DTC

#endif

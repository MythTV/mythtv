#ifndef V2VERSIONINFO_H_
#define V2VERSIONINFO_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"

class V2VersionInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString , Version  )
    SERVICE_PROPERTY2( QString , Branch   )
    SERVICE_PROPERTY2( QString , Protocol )
    SERVICE_PROPERTY2( QString , Binary   )
    SERVICE_PROPERTY2( QString , Schema   )

    public:

        Q_INVOKABLE V2VersionInfo(QObject *parent = nullptr)
                      : QObject         ( parent )
        {
        }

        void Copy( const V2VersionInfo *src )
        {
            m_Version  = src->m_Version ;
            m_Branch   = src->m_Branch  ;
            m_Protocol = src->m_Protocol;
            m_Binary   = src->m_Binary  ;
            m_Schema   = src->m_Schema  ;
        }

    private:
        Q_DISABLE_COPY(V2VersionInfo)
};

Q_DECLARE_METATYPE(V2VersionInfo*)

#endif // VERSIONINFO_H_

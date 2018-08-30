#ifndef KEYBIND_H_
#define KEYBIND_H_

#include <QString>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

/////////////////////////////////////////////////////////////////////////////

class SERVICE_PUBLIC KeyBind : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString         Action          READ Action           WRITE setAction         )
    Q_PROPERTY( QString         Description     READ Description      WRITE setDescription    )
    Q_PROPERTY( QString         KeyList         READ KeyList          WRITE setKeyList        )
    Q_PROPERTY( QString         HostName        READ HostName         WRITE setHostName       )
    Q_PROPERTY( QString         Context         READ Context          WRITE setContext        )

    PROPERTYIMP    ( QString    , Action         )
    PROPERTYIMP    ( QString    , Description    )
    PROPERTYIMP    ( QString    , KeyList        )
    PROPERTYIMP    ( QString    , HostName       )
    PROPERTYIMP    ( QString    , Context        )

    public:

        static inline void InitializeCustomTypes();

    public:

        KeyBind(QObject *parent = 0)
            : QObject         ( parent )
        {
        }

        KeyBind( const KeyBind &src )
        {
            Copy( src );
        }

        void Copy( const KeyBind &src )
        {
            m_Action        = src.m_Action        ;
            m_Description   = src.m_Description   ;
            m_HostName      = src.m_HostName      ;
            m_KeyList       = src.m_KeyList       ;
            m_Context       = src.m_Context       ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::KeyBind  )
Q_DECLARE_METATYPE( DTC::KeyBind* )

namespace DTC
{
inline void KeyBind::InitializeCustomTypes()
{
    qRegisterMetaType< KeyBind   >();
    qRegisterMetaType< KeyBind*  >();
}
}

#endif

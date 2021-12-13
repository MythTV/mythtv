#ifndef V2WOLINFO_H_
#define V2WOLINFO_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"

class V2WOLInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( bool    ,  Enabled   );
    SERVICE_PROPERTY2( int     ,  Reconnect );
    SERVICE_PROPERTY2( int     ,  Retry     );
    SERVICE_PROPERTY2( QString ,  Command   );

    public:

        Q_INVOKABLE V2WOLInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2WOLInfo *src )
        {
            m_Enabled  = src->m_Enabled  ;
            m_Reconnect= src->m_Reconnect;
            m_Retry    = src->m_Retry    ;
            m_Command  = src->m_Command  ;
        }

    private:
        Q_DISABLE_COPY(V2WOLInfo);
};

Q_DECLARE_METATYPE(V2WOLInfo*)

#endif // V2WOLINFO_H_

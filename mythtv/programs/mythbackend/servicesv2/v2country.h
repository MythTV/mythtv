#ifndef V2COUNTRY_H_
#define V2COUNTRY_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"
class V2Country : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString     , Code           )
    SERVICE_PROPERTY2( QString     , Country        )
    SERVICE_PROPERTY2( QString     , NativeCountry  )
    SERVICE_PROPERTY2( QString     , Image          )

  public:
    Q_INVOKABLE V2Country(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    void Copy( const V2Country *src )
    {
        m_Code           = src->m_Code          ;
        m_Country        = src->m_Country       ;
        m_NativeCountry  = src->m_NativeCountry ;
        m_Image          = src->m_Image         ;
    }

    private:
        Q_DISABLE_COPY(V2Country)
};

Q_DECLARE_METATYPE(V2Country*)

#endif // V2COUNTRY_H_

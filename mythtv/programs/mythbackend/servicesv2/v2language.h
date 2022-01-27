#ifndef V2LANGUAGE_H_
#define V2LANGUAGE_H_

#include <QString>
#include "libmythbase/http/mythhttpservice.h"
class V2Language : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version"    , "1.0" );

    SERVICE_PROPERTY2( QString     , Code            )
    SERVICE_PROPERTY2( QString     , Language        )
    SERVICE_PROPERTY2( QString     , NativeLanguage  )
    SERVICE_PROPERTY2( QString     , Image           )

  public:
    Q_INVOKABLE V2Language(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

    void Copy( const V2Language *src )
    {
        m_Code           = src->m_Code           ;
        m_Language       = src->m_Language       ;
        m_NativeLanguage = src->m_NativeLanguage ;
        m_Image          = src->m_Image          ;
    }

    private:
        Q_DISABLE_COPY(V2Language)
};

Q_DECLARE_METATYPE(V2Language*)

#endif // V2LANGUAGE_H_

#ifndef V2LANGUAGELIST_H_
#define V2LANGUAGELIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2language.h"

class V2LanguageList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_CLASSINFO( "Languages", "type=V2Language");

    SERVICE_PROPERTY2( QVariantList, Languages );

    public:

        Q_INVOKABLE V2LanguageList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LanguageList *src )
        {
            CopyListContents< V2Language >( this, m_Languages, src->m_Languages );
        }

        V2Language *AddNewLanguage()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Language( this );
            m_Languages.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2LanguageList);
};

Q_DECLARE_METATYPE(V2LanguageList*)

#endif

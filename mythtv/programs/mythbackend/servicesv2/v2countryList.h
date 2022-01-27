#ifndef V2COUNTRYLIST_H_
#define V2COUNTRYLIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2country.h"

class V2CountryList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    Q_CLASSINFO( "Countries", "type=V2Country");

    SERVICE_PROPERTY2( QVariantList, Countries );

    public:

        Q_INVOKABLE V2CountryList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2CountryList *src )
        {
            CopyListContents< V2Country >( this, m_Countries, src->m_Countries );
        }

        V2Country *AddNewCountry()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2Country( this );
            m_Countries.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2CountryList);
};

Q_DECLARE_METATYPE(V2CountryList*)

#endif

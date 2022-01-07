#ifndef V2RECRULELIST_H_
#define V2RECRULELIST_H_

#include <QVariantList>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2recRule.h"

class V2RecRuleList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "RecRules", "type=V2RecRule");
    Q_CLASSINFO( "AsOf"    , "transient=true"   );

    SERVICE_PROPERTY2( int         , StartIndex      )
    SERVICE_PROPERTY2( int         , Count           )
    SERVICE_PROPERTY2( int         , TotalAvailable  )
    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, RecRules );

    public:

        Q_INVOKABLE V2RecRuleList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2RecRuleList *src )
        {
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2RecRule >( this, m_RecRules, src->m_RecRules );
        }

        V2RecRule *AddNewRecRule()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2RecRule( this );
            m_RecRules.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2RecRuleList);
};

Q_DECLARE_METATYPE(V2RecRuleList*)

#endif

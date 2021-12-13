
#ifndef V2RECRULEFILTERLIST_H_
#define V2RECRULEFILTERLIST_H_

#include <QVariantList>
#include <QDateTime>

#include "libmythbase/http/mythhttpservice.h"

#include "v2recRuleFilter.h"

class V2RecRuleFilterList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "RecRuleFilters", "type=V2RecRuleFilter");
    Q_CLASSINFO( "AsOf"    , "transient=true"   );

    SERVICE_PROPERTY2( int         , StartIndex      )
    SERVICE_PROPERTY2( int         , Count           )
    SERVICE_PROPERTY2( int         , TotalAvailable  )
    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, RecRuleFilters );

    public:

        Q_INVOKABLE V2RecRuleFilterList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2RecRuleFilterList *src )
        {
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< V2RecRuleFilter >( this, m_RecRuleFilters, src->m_RecRuleFilters );
        }

        V2RecRuleFilter *AddNewRecRuleFilter()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2RecRuleFilter( this );
            m_RecRuleFilters.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2RecRuleFilterList);
};

Q_DECLARE_METATYPE(V2RecRuleFilterList*)

#endif

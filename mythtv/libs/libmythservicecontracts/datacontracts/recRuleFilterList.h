 
#ifndef RECRULEFILTERLIST_H_
#define RECRULEFILTERLIST_H_

#include <QVariantList>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "recRuleFilter.h"

namespace DTC
{

class SERVICE_PUBLIC RecRuleFilterList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties.
    // See datacontracthelper.h for details

    Q_CLASSINFO( "RecRuleFilters", "type=DTC::RecRuleFilter");
    Q_CLASSINFO( "AsOf"    , "transient=true"   );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList RecRuleFilters READ RecRuleFilters DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, RecRuleFilters )

    public:

        static inline void InitializeCustomTypes();

    public:

        RecRuleFilterList(QObject *parent = 0)
            : QObject          ( parent ),
              m_StartIndex     ( 0      ),
              m_Count          ( 0      ),
              m_TotalAvailable ( 0      )
        {
        }

        RecRuleFilterList( const RecRuleFilterList &src )
        {
            Copy( src );
        }

        void Copy( const RecRuleFilterList &src )
        {
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< RecRuleFilter >( this, m_RecRuleFilters, src.m_RecRuleFilters );
        }

        RecRuleFilter *AddNewRecRuleFilter()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            RecRuleFilter *pObject = new RecRuleFilter( this );
            m_RecRuleFilters.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecRuleFilterList  )
Q_DECLARE_METATYPE( DTC::RecRuleFilterList* )

namespace DTC
{
inline void RecRuleFilterList::InitializeCustomTypes()
{
    qRegisterMetaType< RecRuleFilterList   >();
    qRegisterMetaType< RecRuleFilterList*  >();

    RecRuleFilter::InitializeCustomTypes();
}
}

#endif

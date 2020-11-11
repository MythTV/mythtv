#ifndef RECRULELIST_H_
#define RECRULELIST_H_

#include <QVariantList>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "recRule.h"

namespace DTC
{

class SERVICE_PUBLIC RecRuleList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "RecRules", "type=DTC::RecRule");
    Q_CLASSINFO( "AsOf"    , "transient=true"   );

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList RecRules READ RecRules )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , TotalAvailable  )
    PROPERTYIMP_REF   ( QDateTime   , AsOf            )
    PROPERTYIMP_REF   ( QString     , Version         )
    PROPERTYIMP_REF   ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, RecRules );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE RecRuleList(QObject *parent = nullptr)
            : QObject          ( parent ),
              m_StartIndex     ( 0      ),
              m_Count          ( 0      ),
              m_TotalAvailable ( 0      )
        {
        }

        void Copy( const RecRuleList *src )
        {
            m_AsOf          = src->m_AsOf           ;
            m_Version       = src->m_Version        ;
            m_ProtoVer      = src->m_ProtoVer       ;

            CopyListContents< RecRule >( this, m_RecRules, src->m_RecRules );
        }

        RecRule *AddNewRecRule()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new RecRule( this );
            m_RecRules.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(RecRuleList);
};

inline void RecRuleList::InitializeCustomTypes()
{
    qRegisterMetaType< RecRuleList*  >();

    RecRule::InitializeCustomTypes();
}

} // namespace DTC

#endif

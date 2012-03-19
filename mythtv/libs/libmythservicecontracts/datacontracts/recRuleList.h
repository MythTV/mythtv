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

    // We need to know the type that will ultimately be contained in 
    // any QVariantList or QVariantMap.  We do his by specifying
    // A Q_CLASSINFO entry with "<PropName>_type" as the key
    // and the type name as the value

    Q_CLASSINFO( "RecRules", "type=DTC::RecRule");

    Q_PROPERTY( int          StartIndex     READ StartIndex      WRITE setStartIndex     )
    Q_PROPERTY( int          Count          READ Count           WRITE setCount          )
    Q_PROPERTY( int          TotalAvailable READ TotalAvailable  WRITE setTotalAvailable )
    Q_PROPERTY( QDateTime    AsOf           READ AsOf            WRITE setAsOf           )
    Q_PROPERTY( QString      Version        READ Version         WRITE setVersion        )
    Q_PROPERTY( QString      ProtoVer       READ ProtoVer        WRITE setProtoVer       )

    Q_PROPERTY( QVariantList RecRules READ RecRules DESIGNABLE true )

    PROPERTYIMP       ( int         , StartIndex      )
    PROPERTYIMP       ( int         , Count           )
    PROPERTYIMP       ( int         , TotalAvailable  )    
    PROPERTYIMP       ( QDateTime   , AsOf            )
    PROPERTYIMP       ( QString     , Version         )
    PROPERTYIMP       ( QString     , ProtoVer        )

    PROPERTYIMP_RO_REF( QVariantList, RecRules )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< RecRuleList   >();
            qRegisterMetaType< RecRuleList*  >();

            RecRule::InitializeCustomTypes();
        }

    public:

        RecRuleList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        RecRuleList( const RecRuleList &src )
        {
            Copy( src );
        }

        void Copy( const RecRuleList &src )
        {
            m_AsOf          = src.m_AsOf           ;
            m_Version       = src.m_Version        ;
            m_ProtoVer      = src.m_ProtoVer       ;

            CopyListContents< RecRule >( this, m_RecRules, src.m_RecRules );
        }

        RecRule *AddNewRecRule()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            RecRule *pObject = new RecRule( this );
            m_RecRules.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::RecRuleList  )
Q_DECLARE_METATYPE( DTC::RecRuleList* )

#endif

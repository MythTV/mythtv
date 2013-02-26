#ifndef LOGMESSAGELIST_H_
#define LOGMESSAGELIST_H_

#include <QVariantList>

#include "serviceexp.h"
#include "datacontracthelper.h"

#include "labelValue.h"
#include "logMessage.h"

namespace DTC
{

class SERVICE_PUBLIC LogMessageList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "HostNames", "type=DTC::LabelValue");
    Q_CLASSINFO( "Applications", "type=DTC::LabelValue");
    Q_CLASSINFO( "LogMessages", "type=DTC::LogMessage");

    Q_PROPERTY( QVariantList HostNames    READ HostNames    DESIGNABLE true )
    Q_PROPERTY( QVariantList Applications READ Applications DESIGNABLE true )
    Q_PROPERTY( QVariantList LogMessages  READ LogMessages  DESIGNABLE true )

    PROPERTYIMP_RO_REF( QVariantList, HostNames    )
    PROPERTYIMP_RO_REF( QVariantList, Applications )
    PROPERTYIMP_RO_REF( QVariantList, LogMessages  )

    public:

        static inline void InitializeCustomTypes();

    public:

        LogMessageList(QObject *parent = 0)
            : QObject( parent )
        {
        }

        LogMessageList( const LogMessageList &src )
        {
            Copy( src );
        }

        void Copy( const LogMessageList &src )
        {
            CopyListContents< LabelValue >( this, m_HostNames,
                                            src.m_HostNames );
            CopyListContents< LabelValue >( this, m_Applications,
                                            src.m_Applications );
            CopyListContents< LogMessage >( this, m_LogMessages,
                                            src.m_LogMessages );
        }

        LabelValue *AddNewHostName()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            LabelValue *pObject = new LabelValue( this );
            m_HostNames.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        LabelValue *AddNewApplication()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            LabelValue *pObject = new LabelValue( this );
            m_Applications.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        LogMessage *AddNewLogMessage()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            LogMessage *pObject = new LogMessage( this );
            m_LogMessages.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LogMessageList  )
Q_DECLARE_METATYPE( DTC::LogMessageList* )

namespace DTC
{
inline void LogMessageList::InitializeCustomTypes()
{
    qRegisterMetaType< LogMessageList   >();
    qRegisterMetaType< LogMessageList*  >();

    LabelValue::InitializeCustomTypes();
    LogMessage::InitializeCustomTypes();
}
}

#endif

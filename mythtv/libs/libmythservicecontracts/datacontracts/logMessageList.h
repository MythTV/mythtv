#ifndef LOGMESSAGELIST_H_
#define LOGMESSAGELIST_H_

#include <QVariantList>

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

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

    Q_PROPERTY( QVariantList HostNames    READ HostNames    )
    Q_PROPERTY( QVariantList Applications READ Applications )
    Q_PROPERTY( QVariantList LogMessages  READ LogMessages  )

    PROPERTYIMP_RO_REF( QVariantList, HostNames    )
    PROPERTYIMP_RO_REF( QVariantList, Applications )
    PROPERTYIMP_RO_REF( QVariantList, LogMessages  );

    public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE LogMessageList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const LogMessageList *src )
        {
            CopyListContents< LabelValue >( this, m_HostNames,
                                            src->m_HostNames );
            CopyListContents< LabelValue >( this, m_Applications,
                                            src->m_Applications );
            CopyListContents< LogMessage >( this, m_LogMessages,
                                            src->m_LogMessages );
        }

        LabelValue *AddNewHostName()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new LabelValue( this );
            m_HostNames.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        LabelValue *AddNewApplication()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new LabelValue( this );
            m_Applications.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        LogMessage *AddNewLogMessage()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new LogMessage( this );
            m_LogMessages.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(LogMessageList);
};

inline void LogMessageList::InitializeCustomTypes()
{
    qRegisterMetaType< LogMessageList*  >();

    LogMessage::InitializeCustomTypes();
    LabelValue::InitializeCustomTypes();
}

} // namespace DTC

#endif

#ifndef V2LOGMESSAGELIST_H_
#define V2LOGMESSAGELIST_H_

#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2logMessage.h"
#include "v2labelValue.h"

class V2LogMessageList : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    // Q_CLASSINFO Used to augment Metadata for properties. 
    // See datacontracthelper.h for details

    Q_CLASSINFO( "HostNames", "type=V2LabelValue");
    Q_CLASSINFO( "Applications", "type=V2LabelValue");
    Q_CLASSINFO( "LogMessages", "type=V2LogMessage");

    SERVICE_PROPERTY2( QVariantList, HostNames    )
    SERVICE_PROPERTY2( QVariantList, Applications )
    SERVICE_PROPERTY2( QVariantList, LogMessages  );

    public:

        Q_INVOKABLE V2LogMessageList(QObject *parent = nullptr)
            : QObject( parent )
        {
        }

        void Copy( const V2LogMessageList *src )
        {
            CopyListContents< V2LabelValue >( this, m_HostNames,
                                            src->m_HostNames );
            CopyListContents< V2LabelValue >( this, m_Applications,
                                            src->m_Applications );
            CopyListContents< V2LogMessage >( this, m_LogMessages,
                                            src->m_LogMessages );
        }

        V2LabelValue *AddNewHostName()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2LabelValue( this );
            m_HostNames.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        V2LabelValue *AddNewApplication()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2LabelValue( this );
            m_Applications.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

        V2LogMessage *AddNewLogMessage()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'

            auto *pObject = new V2LogMessage( this );
            m_LogMessages.append( QVariant::fromValue<QObject *>( pObject ));

            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2LogMessageList);
};

Q_DECLARE_METATYPE(V2LogMessageList*)

#endif // V2LOGMESSAGELIST_H_

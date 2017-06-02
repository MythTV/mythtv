#ifndef FRONTENDACTIONLIST_H
#define FRONTENDACTIONLIST_H

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{
    class SERVICE_PUBLIC FrontendActionList : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("version", "1.0");

        Q_CLASSINFO( "ActionList", "type=QString;name=Action"); // is this legal?

        Q_PROPERTY(QVariantMap ActionList READ ActionList DESIGNABLE true)

        PROPERTYIMP_RO_REF(QVariantMap, ActionList);

      public:
        explicit FrontendActionList(QObject *parent = 0) : QObject(parent)
        {
        }

        void Copy( const FrontendActionList *src)
        {
            m_ActionList = src->m_ActionList;
        }

        private:
        Q_DISABLE_COPY(FrontendActionList);
    };
};

#endif // FRONTENDACTIONLIST_H

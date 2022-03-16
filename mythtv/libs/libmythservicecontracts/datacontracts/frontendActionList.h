#ifndef FRONTENDACTIONLIST_H
#define FRONTENDACTIONLIST_H

#include "libmythservicecontracts/serviceexp.h"
#include "libmythservicecontracts/datacontracthelper.h"

namespace DTC
{
    class SERVICE_PUBLIC FrontendActionList : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("version", "1.0");

        Q_CLASSINFO( "ActionList", "type=QString;name=Action"); // is this legal?

        Q_PROPERTY(QVariantMap ActionList READ ActionList )

        PROPERTYIMP_RO_REF(QVariantMap, ActionList);

      public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit FrontendActionList(QObject *parent = nullptr) : QObject(parent)
        {
        }

        void Copy( const FrontendActionList *src)
        {
            m_ActionList = src->m_ActionList;
        }

        private:
        Q_DISABLE_COPY(FrontendActionList);
    };
inline void FrontendActionList::InitializeCustomTypes()
{
    qRegisterMetaType<FrontendActionList*>();
}

};

#endif // FRONTENDACTIONLIST_H

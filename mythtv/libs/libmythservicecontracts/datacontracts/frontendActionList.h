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

        PROPERTYIMP_RO_REF(QVariantMap, ActionList)

      public:
        static inline void InitializeCustomTypes();

      public:
        FrontendActionList(QObject *parent = 0) : QObject(parent)
        {
        }

        FrontendActionList(const FrontendActionList &src)
        {
            Copy(src);
        }

        void Copy(const FrontendActionList &src)
        {
            m_ActionList = src.m_ActionList;
        }
    };
};

Q_DECLARE_METATYPE(DTC::FrontendActionList)
Q_DECLARE_METATYPE(DTC::FrontendActionList*)

namespace DTC
{
inline void FrontendActionList::InitializeCustomTypes()
{
    qRegisterMetaType<FrontendActionList>();
    qRegisterMetaType<FrontendActionList*>();
}
}

#endif // FRONTENDACTIONLIST_H

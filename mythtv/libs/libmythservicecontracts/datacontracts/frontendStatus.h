#ifndef FRONTENDSTATUS_H
#define FRONTENDSTATUS_H

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{
    class SERVICE_PUBLIC FrontendStatus : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("version", "1.0");

        Q_CLASSINFO("State_type", "QString");
        Q_CLASSINFO("ChapterTimes_type", "Chapter")

        Q_PROPERTY(QVariantMap State READ State DESIGNABLE true)
        Q_PROPERTY(QVariantList ChapterTimes READ ChapterTimes DESIGNABLE true)

        PROPERTYIMP_RO_REF(QVariantMap, State)
        PROPERTYIMP_RO_REF(QVariantList, ChapterTimes)

      public:
        static void InitializeCustomTypes()
        {
            qRegisterMetaType<FrontendStatus>();
            qRegisterMetaType<FrontendStatus*>();
        }

      public:
        FrontendStatus(QObject *parent = 0) : QObject(parent)
        {
        }

        FrontendStatus(const FrontendStatus &src)
        {
            Copy(src);
        }

        void Copy(const FrontendStatus &src)
        {
            m_State = src.m_State;
        }

        void Process(void)
        {
            if (m_State.contains("chaptertimes"))
            {
                if (m_State["chaptertimes"].type() == QVariant::List)
                    m_ChapterTimes = m_State["chaptertimes"].toList();
                m_State.remove("chaptertimes");
            }
        }
    };
};

Q_DECLARE_METATYPE(DTC::FrontendStatus)
Q_DECLARE_METATYPE(DTC::FrontendStatus*)

#endif // FRONTENDSTATUS_H

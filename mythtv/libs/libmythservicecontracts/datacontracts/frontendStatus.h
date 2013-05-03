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

        Q_CLASSINFO( "State", "type=QString");
        Q_CLASSINFO( "ChapterTimes", "type=QString;name=Chapter");
        Q_CLASSINFO( "SubtitleTracks", "type=QString;name=Track");
        Q_CLASSINFO( "AudioTracks", "type=QString;name=Track");

        Q_PROPERTY(QVariantMap State READ State DESIGNABLE true)
        Q_PROPERTY(QVariantList ChapterTimes READ ChapterTimes DESIGNABLE true)
        Q_PROPERTY(QVariantMap SubtitleTracks READ SubtitleTracks DESIGNABLE true)
        Q_PROPERTY(QVariantMap AudioTracks READ AudioTracks DESIGNABLE true)

        PROPERTYIMP_RO_REF(QVariantMap, State)
        PROPERTYIMP_RO_REF(QVariantList, ChapterTimes)
        PROPERTYIMP_RO_REF(QVariantMap, SubtitleTracks)
        PROPERTYIMP_RO_REF(QVariantMap, AudioTracks)

      public:
        static inline void InitializeCustomTypes();

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
            m_State          = src.m_State;
            m_ChapterTimes   = src.m_ChapterTimes;
            m_SubtitleTracks = src.m_SubtitleTracks;
            m_AudioTracks    = src.m_AudioTracks;
        }

        void Process(void)
        {
            if (m_State.contains("chaptertimes"))
            {
                if (m_State["chaptertimes"].type() == QVariant::List)
                    m_ChapterTimes = m_State["chaptertimes"].toList();
                m_State.remove("chaptertimes");
            }

            if (m_State.contains("subtitletracks"))
            {
                if (m_State["subtitletracks"].type() == QVariant::Map)
                    m_SubtitleTracks = m_State["subtitletracks"].toMap();
                m_State.remove("subtitletracks");
            }

            if (m_State.contains("audiotracks"))
            {
                if (m_State["audiotracks"].type() == QVariant::Map)
                    m_AudioTracks = m_State["audiotracks"].toMap();
                m_State.remove("audiotracks");
            }
        }
    };
};

Q_DECLARE_METATYPE(DTC::FrontendStatus)
Q_DECLARE_METATYPE(DTC::FrontendStatus*)

namespace DTC
{
inline void FrontendStatus::InitializeCustomTypes()
{
    qRegisterMetaType<FrontendStatus>();
    qRegisterMetaType<FrontendStatus*>();
}
}

#endif // FRONTENDSTATUS_H

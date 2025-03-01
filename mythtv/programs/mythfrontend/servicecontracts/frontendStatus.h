#ifndef FRONTENDSTATUS_H
#define FRONTENDSTATUS_H

#include "datacontracthelper.h"

namespace DTC
{
    class FrontendStatus : public QObject
    {
        Q_OBJECT
        Q_CLASSINFO("version", "1.1");

        Q_CLASSINFO( "State", "type=QString");
        Q_CLASSINFO( "ChapterTimes", "type=QString;name=Chapter");
        Q_CLASSINFO( "SubtitleTracks", "type=QString;name=Track");
        Q_CLASSINFO( "AudioTracks", "type=QString;name=Track");

        Q_PROPERTY(QString Name READ Name WRITE setName)
        Q_PROPERTY(QString Version READ Version WRITE setVersion)
        Q_PROPERTY(QVariantMap State READ State)
        Q_PROPERTY(QVariantList ChapterTimes READ ChapterTimes)
        Q_PROPERTY(QVariantMap SubtitleTracks READ SubtitleTracks)
        Q_PROPERTY(QVariantMap AudioTracks READ AudioTracks)

        PROPERTYIMP_REF(QString, Name)
        PROPERTYIMP_REF(QString, Version)
        PROPERTYIMP_RO_REF(QVariantMap, State)
        PROPERTYIMP_RO_REF(QVariantList, ChapterTimes)
        PROPERTYIMP_RO_REF(QVariantMap, SubtitleTracks)
        PROPERTYIMP_RO_REF(QVariantMap, AudioTracks)

      public:

        static inline void InitializeCustomTypes();

        Q_INVOKABLE explicit FrontendStatus(QObject *parent = nullptr) : QObject(parent)
        {
        }

        void Copy( const FrontendStatus *src)
        {
            m_Name           = src->m_Name;
            m_Version        = src->m_Version;
            m_State          = src->m_State;
            m_ChapterTimes   = src->m_ChapterTimes;
            m_SubtitleTracks = src->m_SubtitleTracks;
            m_AudioTracks    = src->m_AudioTracks;
        }

        void Process(void)
        {
            if (m_State.contains("chaptertimes"))
            {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                auto type = static_cast<QMetaType::Type>(m_State["chaptertimes"].type());
#else
                auto type = m_State["chaptertimes"].typeId();
#endif
                if (type == QMetaType::QVariantList)
                    m_ChapterTimes = m_State["chaptertimes"].toList();
                m_State.remove("chaptertimes");
            }

            if (m_State.contains("subtitletracks"))
            {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                auto type = static_cast<QMetaType::Type>(m_State["subtitletracks"].type());
#else
                auto type = m_State["subtitletracks"].typeId();
#endif
                if (type == QMetaType::QVariantMap)
                    m_SubtitleTracks = m_State["subtitletracks"].toMap();
                m_State.remove("subtitletracks");
            }

            if (m_State.contains("audiotracks"))
            {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
                auto type = static_cast<QMetaType::Type>(m_State["audiotracks"].type());
#else
                auto type = m_State["audiotracks"].typeId();
#endif
                if (type == QMetaType::QVariantMap)
                    m_AudioTracks = m_State["audiotracks"].toMap();
                m_State.remove("audiotracks");
            }
        }

    private:
        Q_DISABLE_COPY(FrontendStatus);
    };
inline void FrontendStatus::InitializeCustomTypes()
{
    qRegisterMetaType<FrontendStatus*>();
}

};

#endif // FRONTENDSTATUS_H

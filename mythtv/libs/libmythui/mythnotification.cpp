//
//  mythnotification.cpp
//  MythTV
//
//  Created by Jean-Yves Avenard on 25/06/13.
//  Copyright (c) 2013 Bubblestuff Pty Ltd. All rights reserved.
//

// Qt
#include <QCoreApplication>
#include <QTime>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythnotification.h"

const QEvent::Type MythNotification::kNew     = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kUpdate  = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kInfo    = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kError   = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kWarning = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kCheck   = static_cast<QEvent::Type>(QEvent::registerEventType());
const QEvent::Type MythNotification::kBusy    = static_cast<QEvent::Type>(QEvent::registerEventType());

MythNotification::MythNotification(Type nType, void* Parent)
  : MythEvent(nType, "NOTIFICATION"),
    m_parent(Parent)
{
}

MythNotification::MythNotification(int Id, void* Parent)
  : MythEvent(kUpdate, "NOTIFICATION"),
    m_id(Id),
    m_parent(Parent)
{
}

MythNotification::MythNotification(const QString& Title, const QString& Author,
                                   const QString& Details)
  : MythEvent(kNew, "NOTIFICATION"),
    m_description(Title),
    m_metadata({{"minm", Title}, {"asar", Author}, {"asal", Details}})
{
    ToStringList();
}

MythNotification::MythNotification(Type nType, const QString& Title, const QString& Author,
                                   const QString& Details, const QString& Extra)
  : MythEvent(nType, "NOTIFICATION"),
    m_description(Title),
    m_metadata({{"minm", Title}, {"asar", Author}, {"asal", Details}, {"asfm", Extra}})
{
    ToStringList();
}

MythNotification::MythNotification(Type nType, DMAP Metadata)
  : MythEvent(nType, "NOTIFICATION"),
    m_metadata(std::move(Metadata))
{
    ToStringList();
}

MythNotification::MythNotification(const MythEvent& Event)
  : MythEvent(Event)
{
    FromStringList();
}

MythNotification::MythNotification(const MythNotification& Notification)
  : MythEvent(Notification),
    m_id(Notification.m_id),
    m_parent(Notification.m_parent),
    m_fullScreen(Notification.m_fullScreen),
    m_description(Notification.m_description),
    m_duration(Notification.m_duration),
    m_metadata(Notification.m_metadata),
    m_style(Notification.m_style),
    m_visibility(Notification.m_visibility),
    m_priority(Notification.m_priority)
{
    ToStringList();
}

MythEvent* MythNotification::clone() const
{
    return new MythNotification(*this);
}

/*! \brief Contains the application registration id
 *
 * Required to update an existing notification screen owned by an application
 */
void MythNotification::SetId(int Id)
{
    m_id = Id;
    // default registered notification is to not expire
    if (m_id > 0 && m_duration == 0s)
        m_duration = -1s;
}

/*! \brief Contains the parent address. Required if id is set
 * Id provided must match the parent address as provided during the
 * MythNotificationCenter registration, otherwise the id value will be
 * ignored
 */
void MythNotification::SetParent(void* Parent)
{
    m_parent = Parent;
}

/*! \brief A notification may request to be displayed in full screen,
 * this request may not be fullfilled should the theme not handle full screen
 * notification
 */
void MythNotification::SetFullScreen(bool FullScreen)
{
    m_fullScreen = FullScreen;
    ToStringList();
}

/*! \brief Contains a short description of the notification
 */
void MythNotification::SetDescription(const QString& Description)
{
    m_description = Description;
    ToStringList();
}

/*! \brief metadata of the notification.
 * In DMAP format. DMAP can contains various information such as artist,
 * album name, author name, genre etc..
 */
void MythNotification::SetMetaData(const DMAP& MetaData)
{
    m_metadata = MetaData;
    ToStringList();
}

/*! \brief Contains a duration during which the notification will be displayed for.
 * The duration is informative only as the MythNotificationCenter will
 * determine automatically how long a notification can be displayed for
 * and will depend on priority, visibility and other factors
 */
void MythNotification::SetDuration(std::chrono::seconds Duration)
{
    m_duration = Duration;
    ToStringList();
}

/*! \brief Contains an alternative notification style.
 * Should a style be defined, the Notification Center will attempt to load
 * an alternative theme and fall back to the default one if unsuccessful
 */
void MythNotification::SetStyle(const QString& sStyle)
{
    m_style = sStyle;
    ToStringList();
}

/*! \brief Define a bitmask of Visibility
 */
void MythNotification::SetVisibility(VNMask nVisibility)
{
    m_visibility = nVisibility;
    ToStringList();
}

/*! \brief Reserved for future use, not implemented at this stage
 */
void MythNotification::SetPriority(Priority nPriority)
{
    m_priority = nPriority;
    ToStringList();
}

void MythNotification::ToStringList()
{
    m_extradata.clear();
    m_extradata << QString::number(Type())
                << QString::number(static_cast<int>(m_fullScreen))
                << m_description
                << QString::number(m_duration.count())
                << m_style
                << QString::number(m_visibility)
                << QString::number(m_priority)
                << m_metadata.value("minm")
                << m_metadata.value("asar")
                << m_metadata.value("asal")
                << m_metadata.value("asfm");
}

bool MythNotification::FromStringList()
{
    if (m_extradata.size() != 11)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythNotification::FromStringList called with %1 items, expecting 11. '%2'")
            .arg(m_extradata.size()).arg(m_extradata.join(",")));
        return false;
    }

    QStringList::const_iterator it = m_extradata.cbegin();
    Type type = static_cast<Type>((*it++).toInt());
    if (type != Type())
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MythNotification::FromStringList called with type '%1' "
                    "in StringList, expected '%2' as set in constructor.")
            .arg(type).arg(Type()));
        return false;
    }
    m_fullScreen  = ((*it++).toInt() != 0);
    m_description = *it++;
    m_duration    = std::chrono::seconds((*it++).toInt());
    m_style       = *it++;
    m_visibility  = static_cast<VNMask>((*it++).toInt());
    m_priority    = static_cast<Priority>((*it++).toInt());
    m_metadata["minm"] = *it++;
    m_metadata["asar"] = *it++;
    m_metadata["asal"] = *it++;
    m_metadata["asfm"] = *it++;
    return true;
}


/*! \brief Create a string in the format HH:mm:ss from a duration in seconds.
 *
 * HH: will not be displayed if there's less than one hour.
 */
QString MythPlaybackNotification::StringFromSeconds(std::chrono::seconds Time)
{
    QTime ltime = QTime(0,0).addSecs(Time.count());
    return ltime.toString(ltime.hour() > 0 ? "HH:mm:ss" : "mm:ss");
}

MythNotification::Type MythNotification::TypeFromString(const QString& Type)
{
    if (Type == "error")   return MythNotification::kError;
    if (Type == "warning") return MythNotification::kWarning;
    if (Type == "check")   return MythNotification::kCheck;
    if (Type == "busy")    return MythNotification::kBusy;
    return MythNotification::kNew;
}

MythImageNotification::MythImageNotification(Type nType, QImage Image)
  : MythNotification(nType),
    m_image(std::move(Image))
{
}

MythImageNotification::MythImageNotification(Type nType, QString ImagePath)
  : MythNotification(nType),
    m_imagePath(std::move(ImagePath))
{
}

MythImageNotification::MythImageNotification(Type nType, QImage Image, const DMAP& Metadata)
  : MythNotification(nType, Metadata),
    m_image(std::move(Image))
{
}

MythImageNotification::MythImageNotification(Type nType, QString ImagePath, const DMAP& Metadata)
  : MythNotification(nType, Metadata),
    m_imagePath(std::move(ImagePath))
{
}

MythEvent* MythImageNotification::clone() const
{
    return new MythImageNotification(*this);
}

MythPlaybackNotification::MythPlaybackNotification(Type nType, float Progress,
                                                   QString ProgressText)
  : MythNotification(nType),
    m_progress(Progress),
    m_progressText(std::move(ProgressText))
{
}

MythPlaybackNotification::MythPlaybackNotification(Type nType, float Progress,
                                                   QString ProgressText,
                         const DMAP& Metadata)
  : MythNotification(nType, Metadata),
    m_progress(Progress),
    m_progressText(std::move(ProgressText))
{
}

MythPlaybackNotification::MythPlaybackNotification(Type nType,
                                                   std::chrono::seconds Duration,
                                                   int Position)
  : MythNotification(nType),
    m_progress(static_cast<float>(Position) /  static_cast<float>(Duration.count())),
    m_progressText(StringFromSeconds(Duration))
{
}

MythEvent* MythPlaybackNotification::clone() const
{
    return new MythPlaybackNotification(*this);
}

MythMediaNotification::MythMediaNotification(Type nType, const QImage& Image, const DMAP& Metadata,
                                             float Progress, const QString& DurationText)
  : MythNotification(nType, Metadata),
    MythImageNotification(nType, Image),
    MythPlaybackNotification(nType, Progress, DurationText)
{
}

MythMediaNotification::MythMediaNotification(Type nType, const QImage& Image, const DMAP& Metadata,
                                             std::chrono::seconds Duration, int Position)
  : MythNotification(nType, Metadata),
    MythImageNotification(nType, Image),
    MythPlaybackNotification(nType, Duration, Position)
{
}

MythMediaNotification::MythMediaNotification(Type nType, const QString& Image, const DMAP& Metadata,
                                             float Progress, const QString& DurationText)
  : MythNotification(nType, Metadata),
    MythImageNotification(nType, Image),
    MythPlaybackNotification(nType, Progress, DurationText)
{
}

MythMediaNotification::MythMediaNotification(Type nType, const QString& Image, const DMAP& Metadata,
                                             std::chrono::seconds Duration, int Position)
  : MythNotification(nType, Metadata),
    MythImageNotification(nType, Image),
    MythPlaybackNotification(nType, Duration, Position)
{
}

MythMediaNotification::MythMediaNotification(const MythMediaNotification& Notification)
  : MythNotification(Notification),
    MythImageNotification(Notification),
    MythPlaybackNotification(Notification)
{
}

MythEvent* MythMediaNotification::clone() const
{
    return new MythMediaNotification(*this);
}

MythErrorNotification::MythErrorNotification(const QString& Title, const QString& Author,
                                             const QString& Details)
  : MythNotification(kError, Title, Author, Details)
{
    SetDuration(10s);
}

MythWarningNotification::MythWarningNotification(const QString& Title, const QString& Author,
                                                 const QString& Details)
  : MythNotification(kWarning, Title, Author, Details)
{
    SetDuration(10s);
}

MythCheckNotification::MythCheckNotification(const QString& Title, const QString& Author,
                                             const QString& Details)
  : MythNotification(kCheck, Title, Author, Details)
{
    SetDuration(5s);
}

MythBusyNotification::MythBusyNotification(const QString& Title, const QString& Author,
                                           const QString& Details)
  : MythNotification(kBusy, Title, Author, Details)
{
}

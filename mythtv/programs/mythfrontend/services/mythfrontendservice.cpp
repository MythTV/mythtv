// C++
#include <thread>
#include <algorithm>

// Qt
#include <QKeyEvent>
#include <QGlobalStatic>
#include <QCoreApplication>

// MythTV
#include "libmythbase/http/mythhttpmetaservice.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythmetadata/videoutils.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/tv_actions.h"
#include "libmythtv/tv_play.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuistatetracker.h"

// MythFrontend
#include "keybindings.h"
#include "services/mythfrontendservice.h"

#define LOC QString("FrontendServices: ")
using ActionDescs = QHash<QString,QStringList>;

/*! \class FrontendActions
 *
 * A private, static singleton that performs thread safe initialisation of the
 * available actions when first required.
*/
class FrontendActions
{
  public:
    const QStringList& Actions()       { return m_actions; }
    const QStringList& SelectActions() { return m_selectActions; }
    const QStringList& ValueActions()  { return m_valueActions; }
    const ActionDescs& Descriptions()  { return m_actionDescriptions; }

    FrontendActions()
    {
        // Build action and action descriptions
        if (auto * bindings = new KeyBindings(gCoreContext->GetHostName()))
        {
            QStringList contexts = bindings->GetContexts();
            contexts.sort();
            for (const QString & context : std::as_const(contexts))
            {
                m_actionDescriptions[context].clear();
                QStringList ctxactions = bindings->GetActions(context);
                ctxactions.sort();
                m_actions << ctxactions;
                for (const QString & actions : std::as_const(ctxactions))
                {
                    QString desc = actions + "," + bindings->GetActionDescription(context, actions);
                    m_actionDescriptions[context].append(desc);
                }
            }
            delete bindings;
        }
        m_actions.removeDuplicates();
        m_actions.sort();

        // Build actions that have an implicit value (e.g. SELECTSUBTITLE_0)
        for (const auto & action : std::as_const(m_actions))
            if (action.startsWith("select", Qt::CaseInsensitive) && action.contains("_"))
                m_selectActions.append(action);

        // Hardcoded list of value actions
        m_valueActions = QStringList{ ACTION_HANDLEMEDIA,  ACTION_SETVOLUME,
                                      ACTION_SETAUDIOSYNC, ACTION_SETBRIGHTNESS,
                                      ACTION_SETCONTRAST,  ACTION_SETCOLOUR,
                                      ACTION_SETHUE,       ACTION_JUMPCHAPTER,
                                      ACTION_SWITCHTITLE,  ACTION_SWITCHANGLE,
                                      ACTION_SEEKABSOLUTE };
    }

  private:
    QStringList m_actions;
    QStringList m_selectActions;
    QStringList m_valueActions;
    QHash<QString,QStringList> m_actionDescriptions;
};

//NOLINTNEXTLINE(readability-redundant-member-init)
Q_GLOBAL_STATIC(FrontendActions, s_actions)

// This will be initialised in a thread safe manner on first use
Q_GLOBAL_STATIC_WITH_ARGS(MythHTTPMetaService, s_service,
    (FRONTEND_HANDLE, MythFrontendService::staticMetaObject, &MythFrontendService::RegisterCustomTypes))

void MythFrontendService::RegisterCustomTypes()
{
    qRegisterMetaType<FrontendStatus*>("FrontendStatus");
    qRegisterMetaType<FrontendActionList*>("FrontendActionList");
}

MythFrontendService::MythFrontendService()
  : MythHTTPService(s_service)
{
}

FrontendStatus* MythFrontendService::GetStatus()
{
    QVariantMap state;
    MythUIStateTracker::GetFreshState(state);
    auto* result = new FrontendStatus(gCoreContext->GetHostName(), GetMythSourceVersion(), state);
    return result;
}

bool MythFrontendService::SendKey(const QString& Key)
{
    if (Key.isEmpty() || !HasMythMainWindow())
        return false;

    int keycode = 0;
    bool valid = false;
    QString key = Key.toLower();

    // We could statically initialise this mapping of the Qt::Key enum but clients
    // should really just use actions...
    QMetaEnum meta = QMetaEnum::fromType<Qt::Key>();
    for (int i = 0; i < meta.keyCount(); i++)
    {
        if (QByteArray(meta.key(i)).mid(4).toLower() == key)
        {
            keycode = meta.value(i);
            valid = true;
            break;
        }
    }

    if (!valid && key.size() == 1)
    {
        keycode = key.toLatin1()[0] & 0x7f;
        valid = true;
    }

    if (!valid)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Unknown key: '%1'").arg(Key));
        return false;
    }

    MythMainWindow::ResetScreensaver();
    auto * mainwindow = GetMythMainWindow();
    auto * event1 = new QKeyEvent(QEvent::KeyPress, keycode, Qt::NoModifier, "");
    QCoreApplication::postEvent(mainwindow, event1);
    auto * event2 = new QKeyEvent(QEvent::KeyRelease, keycode, Qt::NoModifier, "");
    QCoreApplication::postEvent(mainwindow, event2);
    LOG(VB_HTTP, LOG_INFO, LOC + QString("Sent key: '%1'").arg(Key));
    return true;
}

bool MythFrontendService::SendMessage(const QString& Message, uint Timeout)
{
    if (Message.isEmpty())
        return false;
    QStringList data(QString::number(std::clamp(Timeout, 0U, 1000U)));
    qApp->postEvent(GetMythMainWindow(), new MythEvent(MythEvent::kMythUserMessage, Message, data));
    return true;
}

/*! \brief Send a notification to the frontend.
 *
 * \note Complete use of this method (i.e. using all available parameters) will
 * probably not work as expected as Qt's invoke calls are limited to 10 parameters
 * (one of which is the return type).
*/
bool MythFrontendService::SendNotification(bool  Error,                const QString& Type,
                                           const QString& Message,     const QString& Origin,
                                           const QString& Description, const QString& Image,
                                           const QString& Extra,       const QString& ProgressText,
                                           float Progress,             std::chrono::seconds Timeout,
                                           bool  Fullscreen,           uint  Visibility,
                                           uint  Priority)
{
    if (Message.isEmpty() || !GetNotificationCenter())
        return false;

    ShowNotification(Error ? MythNotification::kError : MythNotification::TypeFromString(Type),
                     Message, Origin.isNull() ? tr("FrontendServices") : Origin,
                     Description, Image, Extra, ProgressText, Progress, Timeout,
                     Fullscreen, Visibility, static_cast<MythNotification::Priority>(Priority));
    return true;
}

FrontendActionList* MythFrontendService::GetActionList(const QString& Context)
{
    QVariantMap result;
    QHashIterator<QString,QStringList> contexts(s_actions->Descriptions());
    while (contexts.hasNext())
    {
        contexts.next();
        if (!Context.isEmpty() && contexts.key() != Context)
            continue;

        // TODO can we keep the context data with QMap<QString, QStringList>?
        QStringList actions = contexts.value();
        for (const QString & action : std::as_const(actions))
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
                result.insert(split[0], split[1]);
        }
    }
    return new FrontendActionList(result);
}

QStringList MythFrontendService::GetContextList()
{
    return s_actions->Descriptions().keys();
}

bool MythFrontendService::SendAction(const QString& Action, const QString& Value, uint Width, uint Height)
{
    if (!HasMythMainWindow() || !IsValidAction(Action))
        return false;

    if (!Value.isEmpty() && s_actions->ValueActions().contains(Action))
    {
        MythMainWindow::ResetScreensaver();
        auto * me = new MythEvent(Action, QStringList(Value));
        qApp->postEvent(GetMythMainWindow(), me);
        return true;
    }

    if (ACTION_SCREENSHOT == Action)
    {
        if (!Width || !Height)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid screenshot parameters.");
            return false;
        }

        QStringList args;
        args << QString::number(Width) << QString::number(Height);
        auto* me = new MythEvent(Action, args);
        qApp->postEvent(GetMythMainWindow(), me);
        return true;
    }

    MythMainWindow::ResetScreensaver();
    qApp->postEvent(GetMythMainWindow(), new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, Action));
    return true;
}

bool MythFrontendService::IsValidAction(const QString& Action)
{
    if (s_actions->Actions().contains(Action) || s_actions->SelectActions().contains(Action))
        return true;
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Action '%1'' is invalid.").arg(Action));
    return false;
}

bool MythFrontendService::PlayRecording(int RecordedId, int ChanId, const QDateTime& StartTime)
{
    QDateTime starttime = StartTime;

    if ((RecordedId <= 0) && (ChanId <= 0 || !StartTime.isValid()))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Recorded ID or Channel ID and StartTime appears invalid.");
        return false;
    }

    if (RecordedId > 0)
    {
        RecordingInfo recInfo = RecordingInfo(static_cast<uint>(RecordedId));
        ChanId = static_cast<int>(recInfo.GetChanID());
        starttime = recInfo.GetRecordingStartTime();
    }

    // Note: This is inconsistent with PlayVideo behaviour - which checks for
    // a current instance of TV and returns false if something is already playing.
    if (GetMythUI()->GetCurrentLocation().toLower() == "playback")
    {
        QString message = QString("NETWORK_CONTROL STOP");
        MythEvent me(message);
        gCoreContext->dispatch(me);

        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(10000) && (GetMythUI()->GetCurrentLocation().toLower() == "playback"))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox")
    {
        GetMythMainWindow()->JumpTo("TV Recording Playback");
        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(10000) && (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox"))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        timer.start();
        while (!timer.hasExpired(10000) && (!MythMainWindow::IsTopScreenInitialized()))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (GetMythUI()->GetCurrentLocation().toLower() == "playbackbox")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("PlayRecording, ChanID: %1 StartTime: %2")
            .arg(ChanId).arg(starttime.toString(Qt::ISODate)));

        QString message = QString("NETWORK_CONTROL PLAY PROGRAM %1 %2 %3")
            .arg(ChanId).arg(starttime.toString("yyyyMMddhhmmss"), "12345");

        MythEvent me(message);
        gCoreContext->dispatch(me);
        return true;
    }

    return false;
}

bool MythFrontendService::PlayVideo(const QString& Id, bool UseBookmark)
{
    if (TV::IsTVRunning())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Ignoring PlayVideo request - frontend is busy."));
        return false;
    }

    bool ok = false;
    uint id = Id.toUInt(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid video Id."));
        return false;
    }

    auto metadata = VideoMetadataListManager::loadOneFromDatabase(id);
    if (!metadata)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Didn't find any video metadata."));
        return false;
    }

    if (metadata->GetHost().isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No host for video."));
        return false;
    }

    QString mrl = generate_file_url("Videos", metadata->GetHost(), metadata->GetFilename());
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("PlayVideo ID: %1 UseBookmark: %2 URL: '%3'")
        .arg(id).arg(UseBookmark).arg(mrl));

    QStringList args;
    args <<  mrl << metadata->GetPlot() <<  metadata->GetTitle()
         << metadata->GetSubtitle() << metadata->GetDirector()
         << QString::number(metadata->GetSeason())
         << QString::number(metadata->GetEpisode())
         << metadata->GetInetRef() << QString::number(metadata->GetLength().count())
         << QString::number(metadata->GetYear())
         << QString::number(metadata->GetID())
         << QString::number(static_cast<int>(UseBookmark));

    auto * me = new MythEvent(ACTION_HANDLEMEDIA, args);
    qApp->postEvent(GetMythMainWindow(), me);
    return true;
}

FrontendStatus::FrontendStatus(QString Name, QString Version, QVariantMap State)
    : m_Name(std::move(Name)),
      m_Version(std::move(Version)),
      m_State(std::move(State))
{
    if (m_State.contains("chaptertimes") &&
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_State["chaptertimes"].type() == QVariant::List
#else
        m_State["chaptertimes"].typeId() == QMetaType::QVariantList
#endif
        )
    {
        m_ChapterTimes = m_State["chaptertimes"].toList();
        m_State.remove("chaptertimes");
    }

    if (m_State.contains("subtitletracks") &&
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_State["subtitletracks"].type() == QVariant::Map
#else
        m_State["subtitletracks"].typeId() == QMetaType::QVariantMap
#endif
        )
    {
        m_SubtitleTracks = m_State["subtitletracks"].toMap();
        m_State.remove("subtitletracks");
    }

    if (m_State.contains("audiotracks") &&
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        m_State["audiotracks"].type() == QVariant::Map
#else
        m_State["audiotracks"].typeId() == QMetaType::QVariantMap
#endif
        )
    {
        m_AudioTracks = m_State["audiotracks"].toMap();
        m_State.remove("audiotracks");
    }
}

FrontendActionList::FrontendActionList(QVariantMap List)
    : m_ActionList(std::move(List))
{
}

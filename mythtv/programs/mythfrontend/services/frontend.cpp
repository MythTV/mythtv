#include <QCoreApplication>
#include <QKeyEvent>

#include <unistd.h> // for usleep

#include "mythcorecontext.h"
#include "keybindings.h"
#include "mythlogging.h"
#include "mythevent.h"
#include "mythuistatetracker.h"
#include "mythuihelper.h"
#include "mythmainwindow.h"
#include "tv_play.h"
#include "mythuinotificationcenter.h"

#include "videometadatalistmanager.h"
#include "videometadata.h"
#include "videoutils.h"

#include "frontend.h"

#define LOC QString("Frontend API: ")

QStringList Frontend::gActionList = QStringList();
QHash<QString,QStringList> Frontend::gActionDescriptions = QHash<QString,QStringList>();

DTC::FrontendStatus* Frontend::GetStatus(void)
{
    DTC::FrontendStatus *status = new DTC::FrontendStatus();
    MythUIStateTracker::GetFreshState(status->State());
    status->Process();
    return status;
}

bool Frontend::SendMessage(const QString &Message, uint Timeout)
{
    if (Message.isEmpty())
        return false;

    QStringList data;
    if (Timeout > 0 && Timeout < 1000)
        data << QString::number(Timeout);
    qApp->postEvent(GetMythMainWindow(),
                    new MythEvent(MythEvent::MythUserMessage, Message,
                    data));
    return true;
}

bool  Frontend::SendNotification(bool  Error,
                                 const QString &Type,
                                 const QString &Message,
                                 const QString &Origin,
                                 const QString &Description,
                                 const QString &Image,
                                 const QString &Extra,
                                 const QString &ProgressText,
                                 float Progress,
                                 int   Timeout,
                                 bool  Fullscreen,
                                 uint  Visibility,
                                 uint  Priority)
{
    if (Message.isEmpty())
        return false;
    if (!GetNotificationCenter())
        return false;

    ShowNotification(Error ? MythNotification::Error :
                             MythNotification::TypeFromString(Type),
                     Message,
                     Origin.isNull() ? tr("FrontendServices") : Origin,
                     Description, Image, Extra,
                     ProgressText, Progress, Timeout,
                     Fullscreen, Visibility, (MythNotification::Priority)Priority);
    return true;
}

bool Frontend::SendAction(const QString &Action, const QString &Value,
                          uint Width, uint Height)
{
    if (!IsValidAction(Action))
        return false;

    static const QStringList value_actions =
        QStringList() << ACTION_HANDLEMEDIA  << ACTION_SETVOLUME <<
                         ACTION_SETAUDIOSYNC << ACTION_SETBRIGHTNESS <<
                         ACTION_SETCONTRAST  << ACTION_SETCOLOUR <<
                         ACTION_SETHUE       << ACTION_JUMPCHAPTER <<
                         ACTION_SWITCHTITLE  << ACTION_SWITCHANGLE <<
                         ACTION_SEEKABSOLUTE;

    if (!Value.isEmpty() && value_actions.contains(Action))
    {
        MythEvent* me = new MythEvent(Action, QStringList(Value));
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
        MythEvent* me = new MythEvent(Action, args);
        qApp->postEvent(GetMythMainWindow(), me);
        return true;
    }

    QKeyEvent* ke = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, Action);
    qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
    return true;
}

bool Frontend::PlayRecording(int ChanID, const QDateTime &StartTime)
{
    QDateTime starttime = StartTime;

    if (!starttime.isValid() || ChanID <= 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Invalid parameters.");
        return false;
    }

    if (GetMythUI()->GetCurrentLocation().toLower() == "playback")
    {
        QString message = QString("NETWORK_CONTROL STOP");
        MythEvent me(message);
        gCoreContext->dispatch(me);

        QTime timer;
        timer.start();
        while ((timer.elapsed() < 10000) &&
               (GetMythUI()->GetCurrentLocation().toLower() == "playback"))
            usleep(10000);
    }

    if (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox")
    {
        GetMythMainWindow()->JumpTo("TV Recording Playback");

        QTime timer;
        timer.start();
        while ((timer.elapsed() < 10000) &&
               (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox"))
            usleep(10000);

        timer.start();
        while ((timer.elapsed() < 10000) &&
               (!GetMythUI()->IsTopScreenInitialized()))
            usleep(10000);
    }

    if (GetMythUI()->GetCurrentLocation().toLower() == "playbackbox")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("PlayRecording, ChanID: %1 StartTime: %2")
            .arg(ChanID).arg(starttime.toString(Qt::ISODate)));

        QString message = QString("NETWORK_CONTROL PLAY PROGRAM %1 %2 %3")
            .arg(ChanID)
            .arg(starttime.toString("yyyyMMddhhmmss"))
            .arg("12345");

        MythEvent me(message);
        gCoreContext->dispatch(me);
        return true;
    }

    return false;
}

bool Frontend::PlayVideo(const QString &Id, bool UseBookmark)
{
    if (TV::IsTVRunning())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Ignoring PlayVideo request - frontend is busy."));
        return false;
    }

    bool ok;
    quint64 id = Id.toUInt(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Invalid video Id."));
        return false;
    }

    VideoMetadataListManager::VideoMetadataPtr metadata =
                     VideoMetadataListManager::loadOneFromDatabase(id);

    if (!metadata)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Didn't find any video metadata."));
        return false;
    }

    if (metadata->GetHost().isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("No host for video."));
        return false;
    }

    QString mrl = generate_file_url("Videos", metadata->GetHost(),
                                    metadata->GetFilename());
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("PlayVideo, id: %1 usebookmark: %2 url: '%3'")
        .arg(id).arg(UseBookmark).arg(mrl));

    QStringList args;
    args <<  mrl << metadata->GetPlot() <<  metadata->GetTitle()
         << metadata->GetSubtitle() << metadata->GetDirector()
         << QString::number(metadata->GetSeason())
         << QString::number(metadata->GetEpisode())
         << metadata->GetInetRef() << QString::number(metadata->GetLength())
         << QString::number(metadata->GetYear())
         << QString::number(metadata->GetID())
         << QString::number(UseBookmark);

    MythEvent *me = new MythEvent(ACTION_HANDLEMEDIA, args);
    qApp->postEvent(GetMythMainWindow(), me);

    return true;
}

QStringList Frontend::GetContextList(void)
{
    InitialiseActions();
    return gActionDescriptions.keys();
}

DTC::FrontendActionList* Frontend::GetActionList(const QString &Context)
{
    DTC::FrontendActionList *list = new DTC::FrontendActionList();

    InitialiseActions();

    QHashIterator<QString,QStringList> contexts(gActionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
        if (!Context.isEmpty() && contexts.key() != Context)
            continue;

        // TODO can we keep the context data with QMap<QString, QStringList>?
        QStringList actions = contexts.value();
        foreach (QString action, actions)
        {
            QStringList split = action.split(",");
            if (split.size() == 2)
                list->ActionList().insert(split[0], split[1]);
        }
    }
    return list;
}

bool Frontend::IsValidAction(const QString &Action)
{
    InitialiseActions();
    if (gActionList.contains(Action))
        return true;

    // TODO There must be a better way to do this
    if (Action.startsWith("SELECTSUBTITLE_") ||
        Action.startsWith("SELECTTTC_")      ||
        Action.startsWith("SELECTCC608_")    ||
        Action.startsWith("SELECTCC708_")    ||
        Action.startsWith("SELECTRAWTEXT_")  ||
        Action.startsWith("SELECTAUDIO_"))
    {
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Action '%1'' is invalid.")
        .arg(Action));
    return false;
}

void Frontend::InitialiseActions(void)
{
    static bool initialised = false;
    if (initialised)
        return;

    initialised = true;
    KeyBindings *bindings = new KeyBindings(gCoreContext->GetHostName());
    if (bindings)
    {
        QStringList contexts = bindings->GetContexts();
        contexts.sort();
        foreach (QString context, contexts)
        {
            gActionDescriptions[context] = QStringList();
            QStringList ctx_actions = bindings->GetActions(context);
            ctx_actions.sort();
            gActionList += ctx_actions;
            foreach (QString actions, ctx_actions)
            {
                QString desc = actions + "," +
                               bindings->GetActionDescription(context, actions);
                gActionDescriptions[context].append(desc);
            }
        }
        delete bindings;
    }
    gActionList.removeDuplicates();
    gActionList.sort();

    foreach (QString actions, gActionList)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Action: %1").arg(actions));
}

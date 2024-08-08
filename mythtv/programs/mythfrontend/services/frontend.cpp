// C++
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt
#include <QCoreApplication>
#include <QKeyEvent>
#include <QEvent>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythmetadata/videometadata.h"
#include "libmythmetadata/videometadatalistmanager.h"
#include "libmythmetadata/videoutils.h"
#include "libmythtv/recordinginfo.h"
#include "libmythtv/tv_actions.h"        // for ACTION_JUMPCHAPTER, etc
#include "libmythtv/tv_play.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuiactions.h"     // for ACTION_HANDLEMEDIA, etc
#include "libmythui/mythuihelper.h"
#include "libmythui/mythuistatetracker.h"

// MythFrontend
#include "frontend.h"
#include "keybindings.h"

#define LOC QString("Frontend API: ")

QStringList Frontend::gActionList = QStringList();
QHash<QString,QStringList> Frontend::gActionDescriptions = QHash<QString,QStringList>();

DTC::FrontendStatus* Frontend::GetStatus(void)
{
    auto *status = new DTC::FrontendStatus();
    MythUIStateTracker::GetFreshState(status->State());

    status->setName(gCoreContext->GetHostName());
    status->setVersion(GetMythSourceVersion());

    status->Process();
    return status;
}

bool Frontend::SendMessage(const QString &Message, uint TimeoutInt)
{
    if (Message.isEmpty())
        return false;

    QStringList data;
    auto Timeout = std::chrono::seconds(TimeoutInt);
    if (Timeout > 0s && Timeout < 1000s)
        data << QString::number(Timeout.count());
    qApp->postEvent(GetMythMainWindow(),
                    new MythEvent(MythEvent::kMythUserMessage, Message,
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

    ShowNotification(Error ? MythNotification::kError :
                             MythNotification::TypeFromString(Type),
                     Message,
                     Origin.isNull() ? tr("FrontendServices") : Origin,
                     Description, Image, Extra,
                     ProgressText, Progress, std::chrono::seconds(Timeout),
                     Fullscreen, Visibility, (MythNotification::Priority)Priority);
    return true;
}

bool Frontend::SendAction(const QString &Action, const QString &Value,
                          uint Width, uint Height)
{
    if (!IsValidAction(Action))
        return false;

    static const QStringList kValueActions =
        QStringList() << ACTION_HANDLEMEDIA  << ACTION_SETVOLUME <<
                         ACTION_SETAUDIOSYNC << ACTION_SETBRIGHTNESS <<
                         ACTION_SETCONTRAST  << ACTION_SETCOLOUR <<
                         ACTION_SETHUE       << ACTION_JUMPCHAPTER <<
                         ACTION_SWITCHTITLE  << ACTION_SWITCHANGLE <<
                         ACTION_SEEKABSOLUTE;

    if (!Value.isEmpty() && kValueActions.contains(Action))
    {
        MythMainWindow::ResetScreensaver();
        auto* me = new MythEvent(Action, QStringList(Value));
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
    auto* ke = new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, Action);
    qApp->postEvent(GetMythMainWindow(), (QEvent*)ke);
    return true;
}

bool Frontend::PlayRecording(int RecordedId, int ChanId,
                             const QDateTime &StartTime)
{
    QDateTime starttime = StartTime;

    if ((RecordedId <= 0) &&
        (ChanId <= 0 || !StartTime.isValid()))
        throw QString("Recorded ID or Channel ID and StartTime appears invalid.");

    if (RecordedId > 0)
    {
        RecordingInfo recInfo = RecordingInfo(RecordedId);
        ChanId = recInfo.GetChanID();
        starttime = recInfo.GetRecordingStartTime();
    }

    if (GetMythUI()->GetCurrentLocation().toLower() == "playback")
    {
        QString message = QString("NETWORK_CONTROL STOP");
        MythEvent me(message);
        gCoreContext->dispatch(me);

        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(10000) &&
               (GetMythUI()->GetCurrentLocation().toLower() == "playback"))
            std::this_thread::sleep_for(10ms);
    }

    if (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox")
    {
        GetMythMainWindow()->JumpTo("TV Recording Playback");

        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(10000) &&
               (GetMythUI()->GetCurrentLocation().toLower() != "playbackbox"))
            std::this_thread::sleep_for(10ms);

        timer.start();
        while (!timer.hasExpired(10000) && (!MythMainWindow::IsTopScreenInitialized()))
            std::this_thread::sleep_for(10ms);
    }

    if (GetMythUI()->GetCurrentLocation().toLower() == "playbackbox")
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("PlayRecording, ChanID: %1 StartTime: %2")
            .arg(ChanId).arg(starttime.toString(Qt::ISODate)));

        QString message = QString("NETWORK_CONTROL PLAY PROGRAM %1 %2 %3")
            .arg(QString::number(ChanId),
                 starttime.toString("yyyyMMddhhmmss"),
                 "12345");

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

    bool ok = false;
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
         << metadata->GetInetRef() << QString::number(metadata->GetLength().count())
         << QString::number(metadata->GetYear())
         << QString::number(metadata->GetID())
         << QString::number(static_cast<int>(UseBookmark));

    auto *me = new MythEvent(ACTION_HANDLEMEDIA, args);
    qApp->postEvent(GetMythMainWindow(), me);

    return true;
}

QStringList Frontend::GetContextList(void)
{
    InitialiseActions();
    return gActionDescriptions.keys();
}

DTC::FrontendActionList* Frontend::GetActionList(const QString &lContext)
{
    auto *list = new DTC::FrontendActionList();

    InitialiseActions();

    QHashIterator<QString,QStringList> contexts(gActionDescriptions);
    while (contexts.hasNext())
    {
        contexts.next();
        if (!lContext.isEmpty() && contexts.key() != lContext)
            continue;

        // TODO can we keep the context data with QMap<QString, QStringList>?
        QStringList actions = contexts.value();
        for (const QString & action : std::as_const(actions))
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
    static bool s_initialised = false;
    if (s_initialised)
        return;

    s_initialised = true;
    auto *bindings = new KeyBindings(gCoreContext->GetHostName());
    if (bindings)
    {
        QStringList contexts = bindings->GetContexts();
        contexts.sort();
        for (const QString & context : std::as_const(contexts))
        {
            gActionDescriptions[context] = QStringList();
            QStringList ctx_actions = bindings->GetActions(context);
            ctx_actions.sort();
            gActionList += ctx_actions;
            for (const QString & actions : std::as_const(ctx_actions))
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

    for (const QString & actions : std::as_const(gActionList))
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Action: %1").arg(actions));
}

bool Frontend::SendKey(const QString &sKey)
{
    int keyCode = 0;
    bool ret = false;
    QObject *keyDest = nullptr;
    QKeyEvent *event = nullptr;
    QMap <QString, int> keyMap;
    QString keyText;
    QString msg;

    keyMap["up"]                     = Qt::Key_Up;
    keyMap["down"]                   = Qt::Key_Down;
    keyMap["left"]                   = Qt::Key_Left;
    keyMap["right"]                  = Qt::Key_Right;
    keyMap["home"]                   = Qt::Key_Home;
    keyMap["end"]                    = Qt::Key_End;
    keyMap["enter"]                  = Qt::Key_Enter;
    keyMap["return"]                 = Qt::Key_Return;
    keyMap["pageup"]                 = Qt::Key_PageUp;
    keyMap["pagedown"]               = Qt::Key_PageDown;
    keyMap["escape"]                 = Qt::Key_Escape;
    keyMap["tab"]                    = Qt::Key_Tab;
    keyMap["backtab"]                = Qt::Key_Backtab;
    keyMap["space"]                  = Qt::Key_Space;
    keyMap["backspace"]              = Qt::Key_Backspace;
    keyMap["insert"]                 = Qt::Key_Insert;
    keyMap["delete"]                 = Qt::Key_Delete;
    keyMap["plus"]                   = Qt::Key_Plus;
    keyMap["comma"]                  = Qt::Key_Comma;
    keyMap["minus"]                  = Qt::Key_Minus;
    keyMap["underscore"]             = Qt::Key_Underscore;
    keyMap["period"]                 = Qt::Key_Period;
    keyMap["numbersign"]             = Qt::Key_NumberSign;
    keyMap["poundsign"]              = Qt::Key_NumberSign;
    keyMap["hash"]                   = Qt::Key_NumberSign;
    keyMap["bracketleft"]            = Qt::Key_BracketLeft;
    keyMap["bracketright"]           = Qt::Key_BracketRight;
    keyMap["backslash"]              = Qt::Key_Backslash;
    keyMap["dollar"]                 = Qt::Key_Dollar;
    keyMap["percent"]                = Qt::Key_Percent;
    keyMap["ampersand"]              = Qt::Key_Ampersand;
    keyMap["parenleft"]              = Qt::Key_ParenLeft;
    keyMap["parenright"]             = Qt::Key_ParenRight;
    keyMap["asterisk"]               = Qt::Key_Asterisk;
    keyMap["question"]               = Qt::Key_Question;
    keyMap["slash"]                  = Qt::Key_Slash;
    keyMap["colon"]                  = Qt::Key_Colon;
    keyMap["semicolon"]              = Qt::Key_Semicolon;
    keyMap["less"]                   = Qt::Key_Less;
    keyMap["equal"]                  = Qt::Key_Equal;
    keyMap["greater"]                = Qt::Key_Greater;
    keyMap["f1"]                     = Qt::Key_F1;
    keyMap["f2"]                     = Qt::Key_F2;
    keyMap["f3"]                     = Qt::Key_F3;
    keyMap["f4"]                     = Qt::Key_F4;
    keyMap["f5"]                     = Qt::Key_F5;
    keyMap["f6"]                     = Qt::Key_F6;
    keyMap["f7"]                     = Qt::Key_F7;
    keyMap["f8"]                     = Qt::Key_F8;
    keyMap["f9"]                     = Qt::Key_F9;
    keyMap["f10"]                    = Qt::Key_F10;
    keyMap["f11"]                    = Qt::Key_F11;
    keyMap["f12"]                    = Qt::Key_F12;
    keyMap["f13"]                    = Qt::Key_F13;
    keyMap["f14"]                    = Qt::Key_F14;
    keyMap["f15"]                    = Qt::Key_F15;
    keyMap["f16"]                    = Qt::Key_F16;
    keyMap["f17"]                    = Qt::Key_F17;
    keyMap["f18"]                    = Qt::Key_F18;
    keyMap["f19"]                    = Qt::Key_F19;
    keyMap["f20"]                    = Qt::Key_F20;
    keyMap["f21"]                    = Qt::Key_F21;
    keyMap["f22"]                    = Qt::Key_F22;
    keyMap["f23"]                    = Qt::Key_F23;
    keyMap["f24"]                    = Qt::Key_F24;

    if (sKey.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("SendKey: No Key received"));
        return ret;
    }

    if (GetMythMainWindow())
        keyDest = GetMythMainWindow();
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            LOC + QString("SendKey: Application has no main window"));
        return ret;
    }

    if (keyMap.contains(sKey.toLower()))
    {
        keyCode = keyMap[sKey.toLower()];
        ret = true;
    }
    else if (sKey.size() == 1)
        {
            keyCode = (int) sKey.toLatin1()[0] & 0x7f;
            ret = true;
        }
    else
    {
        msg = QString("SendKey: Unknown Key = '%1'").arg(sKey);
    }

    if (ret)
    {
        MythMainWindow::ResetScreensaver();

        event = new QKeyEvent(QEvent::KeyPress, keyCode, Qt::NoModifier,
                              keyText);
        QCoreApplication::postEvent(keyDest, event);

        event = new QKeyEvent(QEvent::KeyRelease, keyCode, Qt::NoModifier,
                              keyText);
        QCoreApplication::postEvent(keyDest, event);

        msg = QString("SendKey: Sent %1").arg(sKey);
    }

    LOG(VB_UPNP, LOG_INFO, LOC + msg);

    return ret;
}

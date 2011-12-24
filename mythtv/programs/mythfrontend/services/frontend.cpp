#include <QCoreApplication>
#include <QKeyEvent>

#include "mythcorecontext.h"
#include "keybindings.h"
#include "mythlogging.h"
#include "mythevent.h"
#include "mythuistatetracker.h"
#include "mythmainwindow.h"
#include "tv_actions.h"

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

bool Frontend::SendMessage(const QString &Message)
{
    if (Message.isEmpty())
        return false;

    qApp->postEvent(GetMythMainWindow(),
                    new MythEvent(MythEvent::MythUserMessage, Message));
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
    }
    gActionList.removeDuplicates();
    gActionList.sort();

    foreach (QString actions, gActionList)
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Action: %1").arg(actions));
}

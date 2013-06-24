
#include "mythscreentype.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QRunnable>

#include "mythcorecontext.h"
#include "mythobservable.h"
#include "mthreadpool.h"

#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythprogressdialog.h"
#include "mythuigroup.h"
#include "mythlogging.h"

class SemaphoreLocker
{
  public:
    SemaphoreLocker(QSemaphore *lock) : m_lock(lock)
    {
        if (m_lock)
            m_lock->acquire();
    }
    ~SemaphoreLocker()
    {
        if (m_lock)
            m_lock->release();
    }
  private:
    QSemaphore *m_lock;
};

QEvent::Type ScreenLoadCompletionEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

class ScreenLoadTask : public QRunnable
{
  public:
    ScreenLoadTask(MythScreenType &parent) : m_parent(parent) {}

  private:
    void run(void)
    {
        m_parent.Load();
        m_parent.m_IsLoaded = true;
        m_parent.m_IsLoading = false;
        m_parent.m_LoadLock.release();
    }

    MythScreenType &m_parent;
};

MythScreenType::MythScreenType(
    MythScreenStack *parent, const QString &name, bool fullscreen) :
    MythUIType(parent, name), m_LoadLock(1)
{
    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = parent;
    m_BusyPopup = NULL;
    m_IsDeleting = false;
    m_IsLoading = false;
    m_IsLoaded = false;
    m_IsInitialized = false;

    // Can be overridden, of course, but default to full sized.
    m_Area = GetMythMainWindow()->GetUIScreenRect();

    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND)
        gCoreContext->SendSystemEvent(
            QString("SCREEN_TYPE CREATED %1").arg(name));
}

MythScreenType::MythScreenType(
    MythUIType *parent, const QString &name, bool fullscreen) :
    MythUIType(parent, name), m_LoadLock(1)
{
    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = NULL;
    m_BusyPopup = NULL;
    m_IsDeleting = false;
    m_IsLoading = false;
    m_IsLoaded = false;
    m_IsInitialized = false;

    m_Area = GetMythMainWindow()->GetUIScreenRect();

    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND)
        gCoreContext->SendSystemEvent(
                QString("SCREEN_TYPE CREATED %1").arg(name));
}

MythScreenType::~MythScreenType()
{
    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND)
        gCoreContext->SendSystemEvent(
                QString("SCREEN_TYPE DESTROYED %1").arg(objectName()));

    // locking ensures background screen load can finish running
    SemaphoreLocker locker(&m_LoadLock);

    m_CurrentFocusWidget = NULL;
    emit Exiting();
}

bool MythScreenType::IsFullscreen(void) const
{
    return m_FullScreen;
}

void MythScreenType::SetFullscreen(bool full)
{
    m_FullScreen = full;
}

MythUIType *MythScreenType::GetFocusWidget(void) const
{
    return m_CurrentFocusWidget;
}

bool MythScreenType::SetFocusWidget(MythUIType *widget)
{ 
    if (!widget || !widget->IsVisible())
    {
        QMap<int, MythUIType *>::iterator it = m_FocusWidgetList.begin();
        MythUIType *current;

        while (it != m_FocusWidgetList.end())
        {
            current = *it;

            if (current->CanTakeFocus() && current->IsVisible())
            {
                widget = current;
                break;
            }
            ++it;
        }
    }

    if (!widget)
        return false;

    if (m_CurrentFocusWidget == widget)
        return true;
    
    MythUIText *helpText = dynamic_cast<MythUIText *>(GetChild("helptext"));
    if (helpText)
        helpText->Reset();

    if (m_CurrentFocusWidget)
        m_CurrentFocusWidget->LoseFocus();
    m_CurrentFocusWidget = widget;
    m_CurrentFocusWidget->TakeFocus();

    if (helpText && !widget->GetHelpText().isEmpty())
        helpText->SetText(widget->GetHelpText());

    return true;
}

bool MythScreenType::NextPrevWidgetFocus(bool up)
{
    if (!m_CurrentFocusWidget || m_FocusWidgetList.isEmpty())
        return SetFocusWidget(NULL);

    bool reachedCurrent = false;
    bool looped = false;

    QMap<int, MythUIType *>::iterator it = m_FocusWidgetList.begin();
    MythUIType *current;

    // There is probably a more efficient way to do this, but the list
    // is never going to be that big so it will do for now
    if (up)
    {
        while (it != m_FocusWidgetList.end())
        {
            current = *it;

            if ((looped || reachedCurrent) &&
                current->IsVisible() && current->IsEnabled())
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            ++it;

            if (it == m_FocusWidgetList.end())
            {
                if (looped)
                    return false;
                else
                {
                    looped = true;
                    it = m_FocusWidgetList.begin();
                }
            }
        }
    }
    else
    {
        it = m_FocusWidgetList.end() - 1;
        while (it != m_FocusWidgetList.begin() - 1)
        {
            current = *it;

            if ((looped || reachedCurrent) &&
                current->IsVisible() && current->IsEnabled())
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            --it;

            if (it == m_FocusWidgetList.begin() - 1)
            {
                if (looped)
                    return false;
                else
                {
                    looped = true;
                    it = m_FocusWidgetList.end() - 1;
                }
            }
        }
    }

    return false;
}

void MythScreenType::BuildFocusList(void)
{
    m_FocusWidgetList.clear();
    m_CurrentFocusWidget = NULL;

    AddFocusableChildrenToList(m_FocusWidgetList);

    if (m_FocusWidgetList.size() > 0)
        SetFocusWidget();
}

MythScreenStack *MythScreenType::GetScreenStack(void) const
{
    return m_ScreenStack;
}

void MythScreenType::aboutToHide(void)
{
    if (!m_FullScreen)
    {
        if (!GetMythMainWindow()->GetPaintWindow()->mask().isEmpty())
        {
            // remove this screen's area from the mask so any embedded video is
            // shown which was covered by this screen
            if (!m_SavedMask.isEmpty())
                GetMythMainWindow()->GetPaintWindow()->setMask(m_SavedMask);
        }
    }

    ActivateAnimations(MythUIAnimation::AboutToHide);
}

void MythScreenType::aboutToShow(void)
{
    if (!m_FullScreen)
    {
        if (!GetMythMainWindow()->GetPaintWindow()->mask().isEmpty())
        {
            // add this screens area to the mask so any embedded video isn't
            // shown in front of this screen
            QRegion region = GetMythMainWindow()->GetPaintWindow()->mask();
            m_SavedMask = region;
            region = region.unite(QRegion(m_Area));
            GetMythMainWindow()->GetPaintWindow()->setMask(region);
        }
    }

    ActivateAnimations(MythUIAnimation::AboutToShow);
}

bool MythScreenType::IsDeleting(void) const
{
    return m_IsDeleting;
}

void MythScreenType::SetDeleting(bool deleting)
{
    m_IsDeleting = deleting;
}

bool MythScreenType::Create(void)
{
    return true;
}

/**
 * \brief Load data which will ultimately be displayed on-screen or used to
 *        determine what appears on-screen (See Warning)
 *
 * \warning This method should only load data, it should NEVER perform UI
 *          routines or segfaults WILL result. This includes assinging data to
 *          any widgets, calling methods on a widget or anything else which
 *          triggers redraws. The safest and recommended approach is to avoid
 *          any interaction with a libmythui class or class member.
 */
void MythScreenType::Load(void)
{
    // Virtual
}

void MythScreenType::LoadInBackground(QString message)
{
    m_LoadLock.acquire();

    m_IsLoading = true;
    m_IsLoaded = false;

    m_ScreenStack->AllowReInit();

    OpenBusyPopup(message);

    ScreenLoadTask *loadTask = new ScreenLoadTask(*this);
    MThreadPool::globalInstance()->start(loadTask, "ScreenLoad");
}

void MythScreenType::LoadInForeground(void)
{
    SemaphoreLocker locker(&m_LoadLock);

    m_IsLoading = true;
    m_IsLoaded = false;

    m_ScreenStack->AllowReInit();
    Load();
    m_IsLoaded = true;
    m_IsLoading = false;
}

void MythScreenType::ReloadInBackground(void)
{
    m_IsInitialized = false;
    LoadInBackground();
}

void MythScreenType::OpenBusyPopup(QString message)
{
    if (m_BusyPopup)
        return;

    QString msg(tr("Loading..."));
    if (!message.isEmpty())
        msg = message;

    MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
    m_BusyPopup =
        new MythUIBusyDialog(msg, popupStack, "mythscreentypebusydialog");

    if (m_BusyPopup->Create())
        popupStack->AddScreen(m_BusyPopup, false);
}

void MythScreenType::CloseBusyPopup(void)
{
    if (m_BusyPopup)
        m_BusyPopup->Close();
    m_BusyPopup = NULL;
}

void MythScreenType::SetBusyPopupMessage(const QString &message)
{
    if (m_BusyPopup)
        m_BusyPopup->SetMessage(message);
}

void MythScreenType::ResetBusyPopup(void)
{
    if (m_BusyPopup)
        m_BusyPopup->Reset();
}

/**
 * \brief Has Init() been called on this screen?
 */
bool MythScreenType::IsInitialized(void) const
{
    return m_IsInitialized;
}

void MythScreenType::doInit(void)
{
    SemaphoreLocker locker(&m_LoadLock); // don't run while loading..

    CloseBusyPopup();
    Init();
    m_IsInitialized = true;
}

/**
 * \brief Used after calling Load() to assign data to widgets and other UI
 *        initilisation which is prohibited in Load()
 *
 * \warning Do NOT confuse this with Load(), they serve very different purposes
 *          and most often both should be used when creating a new screen.
 */
void MythScreenType::Init(void)
{
    // Virtual
}

void MythScreenType::Close(void)
{
    CloseBusyPopup();
    if (GetScreenStack())
        GetScreenStack()->PopScreen(this);
}

void MythScreenType::ShowMenu(void)
{
    // Virtual
}

static void DoSetTextFromMap(MythUIType *UItype, QHash<QString, QString> &infoMap)
{
    QList<MythUIType *> *children = UItype->GetAllChildren();

    MythUIText *textType;

    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
            textType->SetTextFromMap(infoMap);


        MythUIGroup *group = dynamic_cast<MythUIGroup *> (type);
        if (group)
            DoSetTextFromMap(type, infoMap);
    }
}

void MythScreenType::SetTextFromMap(QHash<QString, QString> &infoMap)
{
    DoSetTextFromMap((MythUIType*) this, infoMap);
}

static void DoResetMap(MythUIType *UItype, QHash<QString, QString> &infoMap)
{
    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = UItype->GetAllChildren();

    MythUIText *textType;
    QMutableListIterator<MythUIType *> i(*children);
    while (i.hasNext())
    {
        MythUIType *type = i.next();
        if (!type->IsVisible())
            continue;

        textType = dynamic_cast<MythUIText *> (type);
        if (textType && infoMap.contains(textType->objectName()))
            textType->Reset();

        MythUIGroup *group = dynamic_cast<MythUIGroup *> (type);
        if (group)
            DoResetMap(type, infoMap);
    }
}

void MythScreenType::ResetMap(QHash<QString, QString> &infoMap)
{
    DoResetMap(this, infoMap);
}

bool MythScreenType::keyPressEvent(QKeyEvent *event)
{
    if (m_CurrentFocusWidget && m_CurrentFocusWidget->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP" || action == "PREVIOUS")
        {
            if (!NextPrevWidgetFocus(false))
                handled = false;
        }
        else if (action == "RIGHT" || action == "DOWN" || action == "NEXT")
        {
            if (!NextPrevWidgetFocus(true))
                handled = false;
        }
        else if (action == "ESCAPE")
            Close();
        else if (action == "MENU")
            ShowMenu();
        else if (action.startsWith("SYSEVENT"))
            gCoreContext->SendSystemEvent(QString("KEY_%1").arg(action.mid(8)));
        else if (action == ACTION_SCREENSHOT)
            GetMythMainWindow()->ScreenShot();
        else if (action == ACTION_TVPOWERON)
            GetMythMainWindow()->HandleTVPower(true);
        else if (action == ACTION_TVPOWEROFF)
            GetMythMainWindow()->HandleTVPower(false);
        else
            handled = false;
    }

    return handled;
}

bool MythScreenType::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;
    if (event->gesture() == MythGestureEvent::Click)
    {
        switch (event->GetButton())
        {
            case MythGestureEvent::RightButton :
                ShowMenu();
                handled = true;
                break;
            default :
                break;
        }

    }

    if (!handled)
    {
        MythUIType *clicked = GetChildAt(event->GetPosition());
        if (clicked && clicked->IsEnabled())
        {
            SetFocusWidget(clicked);
            if (clicked->gestureEvent(event))
                handled = true;
        }
    }

    return handled;
}

/**
 *  \copydoc MythUIType::ParseElement()
 */
bool MythScreenType::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "area")
    {
        MythRect rect = parseRect(element, false);
        MythRect rectN = parseRect(element);
        QRect screenArea = GetMythMainWindow()->GetUIScreenRect();

        if (rect.x() == -1)
            rectN.moveLeft((screenArea.width() - rectN.width()) / 2);

        if (rect.y() == -1)
            rectN.moveTop((screenArea.height() - rectN.height()) / 2);

        SetArea(rectN);

        if (m_Area.width() < screenArea.width() ||
            m_Area.height() < screenArea.height())
        {
            m_FullScreen = false;
        }
        else
        {
            m_FullScreen = true;
        }
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

/**
 *  \copydoc MythUIType::CopyFrom()
 */
void MythScreenType::CopyFrom(MythUIType *base)
{
    MythScreenType *st = dynamic_cast<MythScreenType *>(base);
    if (!st)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    m_FullScreen = st->m_FullScreen;
    m_IsDeleting = false;

    MythUIType::CopyFrom(base);

    ConnectDependants(true);

    BuildFocusList();
};

/**
 * \copydoc MythUIType::CreateCopy()
 *
 * Do not use.
 *
 */
void MythScreenType::CreateCopy(MythUIType *)
{
    LOG(VB_GENERAL, LOG_ERR, "CreateCopy called on screentype - bad.");
}

MythPainter* MythScreenType::GetPainter(void)
{
    if (m_Painter)
        return m_Painter;
    if (m_ScreenStack)
        return m_ScreenStack->GetPainter();
    return GetMythPainter();
}

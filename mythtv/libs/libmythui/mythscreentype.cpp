
#include "mythscreentype.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QInputMethodEvent>
#include <QRunnable>
#include <utility>

#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythobservable.h"

#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythprogressdialog.h"
#include "mythuigroup.h"
#include "mythuistatetype.h"
#include "mythgesture.h"
#include "mythuitext.h"

class SemaphoreLocker
{
  public:
    explicit SemaphoreLocker(QSemaphore *lock) : m_lock(lock)
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
    QSemaphore *m_lock {nullptr};
};

const QEvent::Type ScreenLoadCompletionEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

class ScreenLoadTask : public QRunnable
{
  public:
    explicit ScreenLoadTask(MythScreenType &parent) : m_parent(parent) {}

  private:
    void run(void) override // QRunnable
    {
        m_parent.Load();
        m_parent.m_isLoaded = true;
        m_parent.m_isLoading = false;
        m_parent.m_loadLock.release();
    }

    MythScreenType &m_parent;
};

MythScreenType::MythScreenType(
    MythScreenStack *parent, const QString &name, bool fullscreen) :
    MythUIComposite(parent, name),
    m_fullScreen(fullscreen),
    m_screenStack(parent)
{
    // Can be overridden, of course, but default to full sized.
    m_area = GetMythMainWindow()->GetUIScreenRect();

    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND)
        gCoreContext->SendSystemEvent(
            QString("SCREEN_TYPE CREATED %1").arg(name));
}

MythScreenType::MythScreenType(
    MythUIType *parent, const QString &name, bool fullscreen) :
    MythUIComposite(parent, name),
    m_fullScreen(fullscreen)
{
    m_area = GetMythMainWindow()->GetUIScreenRect();

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
    SemaphoreLocker locker(&m_loadLock);

    m_currentFocusWidget = nullptr;
    emit Exiting();
}

bool MythScreenType::IsFullscreen(void) const
{
    return m_fullScreen;
}

void MythScreenType::SetFullscreen(bool full)
{
    m_fullScreen = full;
}

MythUIType *MythScreenType::GetFocusWidget(void) const
{
    return m_currentFocusWidget;
}

bool MythScreenType::SetFocusWidget(MythUIType *widget)
{
    if (!widget || !widget->IsVisible(true))
    {
        for (auto *current : std::as_const(m_focusWidgetList))
        {
            if (current->CanTakeFocus() && current->IsVisible(true))
            {
                widget = current;
                break;
            }
        }
    }

    if (!widget)
        return false;

    if (m_currentFocusWidget == widget)
        return true;

    MythUIText *helpText = dynamic_cast<MythUIText *>(GetChild("helptext"));
    if (helpText)
        helpText->Reset();

    // Let each 'buttonlist' know the name of the currently active buttonlist
    QString name = widget->GetXMLName();
    for (auto *w : std::as_const(m_focusWidgetList))
        w->SetFocusedName(name);

    if (m_currentFocusWidget)
        m_currentFocusWidget->LoseFocus();
    m_currentFocusWidget = widget;
    m_currentFocusWidget->TakeFocus();

    if (helpText && !widget->GetHelpText().isEmpty())
        helpText->SetText(widget->GetHelpText());

    return true;
}

bool MythScreenType::NextPrevWidgetFocus(bool up)
{
    if (!m_currentFocusWidget || m_focusWidgetList.isEmpty())
        return SetFocusWidget(nullptr);
    if (m_focusWidgetList.size() == 1)
      return false;

    // Run the list from the current pointer to the end/begin and loop
    // around back to itself.  Start by geting an iterator pointing at
    // the current focus (or at the end if the focus isn't in the
    // list).
    auto it = m_focusWidgetList.find(m_currentFocusWidget->m_focusOrder,
                                     m_currentFocusWidget);
    if (up)
    {
        if (it != m_focusWidgetList.end())
            it++;
        if (it == m_focusWidgetList.end())
            it = m_focusWidgetList.begin();
        // Put an upper limit on loops to guarantee exit at some point.
        for (auto count = m_focusWidgetList.size() * 2; count > 0; count--)
        {
            MythUIType *current = *it;
            if (current->IsVisible(true) && current->IsEnabled())
                return SetFocusWidget(current);
            it++;
            if (it == m_focusWidgetList.end())
                it = m_focusWidgetList.begin();
            if (*it == m_currentFocusWidget)
                return false;
        }
    }
    else
    {
        if (it == m_focusWidgetList.begin())
            it = m_focusWidgetList.end();
        // Put an upper limit on loops to guarantee exit at some point.
        for (auto count = m_focusWidgetList.size() * 2; count > 0; count--)
        {
            it--;
            if (*it == m_currentFocusWidget)
                return false;
            MythUIType *current = *it;
            if (current->IsVisible(true) && current->IsEnabled())
                return SetFocusWidget(current);
            if (it == m_focusWidgetList.begin())
                it = m_focusWidgetList.end();
        }
    }

    return false;
}

void MythScreenType::BuildFocusList(void)
{
    m_focusWidgetList.clear();
    m_currentFocusWidget = nullptr;

    AddFocusableChildrenToList(m_focusWidgetList);

    if (!m_focusWidgetList.empty())
        SetFocusWidget();
}

MythScreenStack *MythScreenType::GetScreenStack(void) const
{
    return m_screenStack;
}

void MythScreenType::aboutToHide(void)
{
    if (!m_fullScreen)
    {
        if (!GetMythMainWindow()->GetPaintWindow()->mask().isEmpty())
        {
            // remove this screen's area from the mask so any embedded video is
            // shown which was covered by this screen
            if (!m_savedMask.isEmpty())
                GetMythMainWindow()->GetPaintWindow()->setMask(m_savedMask);
        }
    }

    ActivateAnimations(MythUIAnimation::AboutToHide);
}

void MythScreenType::aboutToShow(void)
{
    if (!m_fullScreen)
    {
        if (!GetMythMainWindow()->GetPaintWindow()->mask().isEmpty())
        {
            // add this screens area to the mask so any embedded video isn't
            // shown in front of this screen
            QRegion region = GetMythMainWindow()->GetPaintWindow()->mask();
            m_savedMask = region;
            region = region.united(QRegion(m_area));
            GetMythMainWindow()->GetPaintWindow()->setMask(region);
        }
    }

    ActivateAnimations(MythUIAnimation::AboutToShow);
}

bool MythScreenType::IsDeleting(void) const
{
    return m_isDeleting;
}

void MythScreenType::SetDeleting(bool deleting)
{
    m_isDeleting = deleting;
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

void MythScreenType::LoadInBackground(const QString& message)
{
    m_loadLock.acquire();

    m_isLoading = true;
    m_isLoaded = false;

    m_screenStack->AllowReInit();

    OpenBusyPopup(message);

    auto *loadTask = new ScreenLoadTask(*this);
    MThreadPool::globalInstance()->start(loadTask, "ScreenLoad");
}

void MythScreenType::LoadInForeground(void)
{
    SemaphoreLocker locker(&m_loadLock);

    m_isLoading = true;
    m_isLoaded = false;

    m_screenStack->AllowReInit();
    Load();
    m_isLoaded = true;
    m_isLoading = false;
}

void MythScreenType::ReloadInBackground(void)
{
    m_isInitialized = false;
    LoadInBackground();
}

void MythScreenType::OpenBusyPopup(const QString& message)
{
    if (m_busyPopup)
        return;

    QString msg(tr("Loading..."));
    if (!message.isEmpty())
        msg = message;

    MythScreenStack *popupStack =
                GetMythMainWindow()->GetStack("popup stack");
    m_busyPopup =
        new MythUIBusyDialog(msg, popupStack, "mythscreentypebusydialog");

    if (m_busyPopup->Create())
        popupStack->AddScreen(m_busyPopup, false);
}

void MythScreenType::CloseBusyPopup(void)
{
    if (m_busyPopup)
        m_busyPopup->Close();
    m_busyPopup = nullptr;
}

void MythScreenType::SetBusyPopupMessage(const QString &message)
{
    if (m_busyPopup)
        m_busyPopup->SetMessage(message);
}

void MythScreenType::ResetBusyPopup(void)
{
    if (m_busyPopup)
        m_busyPopup->Reset();
}

/**
 * \brief Has Init() been called on this screen?
 */
bool MythScreenType::IsInitialized(void) const
{
    return m_isInitialized;
}

void MythScreenType::doInit(void)
{
    SemaphoreLocker locker(&m_loadLock); // don't run while loading..

    CloseBusyPopup();
    Init();
    m_isInitialized = true;
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

bool MythScreenType::inputMethodEvent(QInputMethodEvent *event)
{
    return !GetMythMainWindow()->IsExitingToMain() && m_currentFocusWidget &&
        m_currentFocusWidget->inputMethodEvent(event);
}

bool MythScreenType::keyPressEvent(QKeyEvent *event)
{
    if (!GetMythMainWindow()->IsExitingToMain() && m_currentFocusWidget &&
        m_currentFocusWidget->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
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
        {
            Close();
        }
        else if (action == "MENU")
        {
            ShowMenu();
        }
        else if (action.startsWith("SYSEVENT"))
        {
            gCoreContext->SendSystemEvent(QString("KEY_%1").arg(action.mid(8)));
        }
        else if (action == ACTION_SCREENSHOT)
        {
            MythMainWindow::ScreenShot();
        }
        else if (action == ACTION_TVPOWERON || action == ACTION_TVPOWEROFF)
        {
            GetMythMainWindow()->HandleTVAction(action);
        }
        else
        {
            handled = false;
        }
    }

    return handled;
}

bool MythScreenType::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;
    if (event->GetGesture() == MythGestureEvent::Click)
    {
        switch (event->GetButton())
        {
            case Qt::RightButton:
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

        m_fullScreen = (m_area.width()  >= screenArea.width() &&
                        m_area.height() >= screenArea.height());
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
    auto *st = dynamic_cast<MythScreenType *>(base);
    if (!st)
    {
        LOG(VB_GENERAL, LOG_ERR, "ERROR, bad parsing");
        return;
    }

    m_fullScreen = st->m_fullScreen;
    m_isDeleting = false;

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
void MythScreenType::CreateCopy(MythUIType * /*parent*/)
{
    LOG(VB_GENERAL, LOG_ERR, "CreateCopy called on screentype - bad.");
}

MythPainter* MythScreenType::GetPainter(void)
{
    if (m_painter)
        return m_painter;
    if (m_screenStack)
        return MythScreenStack::GetPainter();
    return GetMythPainter();
}

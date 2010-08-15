
#include "mythscreentype.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QRunnable>
#include <QThreadPool>

#include "mythverbose.h"
#include "mythobservable.h"

#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythprogressdialog.h"

QEvent::Type ScreenLoadCompletionEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

class ScreenLoadTask : public QRunnable
{
  public:
    ScreenLoadTask(MythScreenType *parent) : m_parent(parent) {}

  private:
    void run()
    {
        if (m_parent)
            m_parent->LoadInForeground();
    }

    MythScreenType *m_parent;
};


MythScreenType::MythScreenType(MythScreenStack *parent, const QString &name,
                               bool fullscreen)
              : MythUIType(parent, name)
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
}

MythScreenType::MythScreenType(MythUIType *parent, const QString &name,
                               bool fullscreen)
              : MythUIType(parent, name)
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
}

MythScreenType::~MythScreenType()
{
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

    MythUIText *helpText = dynamic_cast<MythUIText *>(GetChild("helptext"));
    if (helpText)
        helpText->Reset();

    if (m_CurrentFocusWidget)
        m_CurrentFocusWidget->LoseFocus();
    m_CurrentFocusWidget = widget;
    m_CurrentFocusWidget->TakeFocus();

    if (helpText)
        helpText->SetText(m_CurrentFocusWidget->GetHelpText());

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

// This method should only load data, it should never perform UI routines
void MythScreenType::Load(void)
{
    // Virtual
}

void MythScreenType::LoadInBackground(void)
{
    m_IsLoading = true;

    OpenBusyPopup();

    ScreenLoadTask *loadTask = new ScreenLoadTask(this);
    QThreadPool::globalInstance()->start(loadTask);
}

void MythScreenType::LoadInForeground(void)
{
    m_IsLoading = true;
    Load();
    m_IsLoaded = true;
    m_IsLoading = false;
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

bool MythScreenType::IsInitialized(void) const
{
    return m_IsInitialized;
}

void MythScreenType::doInit(void)
{
    CloseBusyPopup();
    Init();
    m_IsInitialized = true;
}

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

void MythScreenType::SetTextFromMap(QHash<QString, QString> &infoMap)
{
    QList<MythUIType *> *children = GetAllChildren();

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
    }
}

void MythScreenType::ResetMap(QHash<QString, QString> &infoMap)
{
    if (infoMap.isEmpty())
        return;

    QList<MythUIType *> *children = GetAllChildren();

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
    }
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
            NextPrevWidgetFocus(false);
        else if (action == "RIGHT" || action == "DOWN" || action == "NEXT")
            NextPrevWidgetFocus(true);
        else if (action == "ESCAPE")
            Close();
        else if (action == "MENU")
            ShowMenu();
        else if (action.startsWith("SYSEVENT"))
        {
            MythEvent me(QString("GLOBAL_SYSTEM_EVENT KEY_%1")
                                 .arg(action.mid(8)));
            QCoreApplication::postEvent(
                GetMythMainWindow()->GetSystemEventHandler(), me.clone());
        }
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
            rectN.setX((screenArea.width() - rectN.width()) / 2);

        if (rect.y() == -1)
            rectN.setY((screenArea.height() - rectN.height()) / 2);

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
        return false;
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
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_FullScreen = st->m_FullScreen;
    m_IsDeleting = false;

    MythUIType::CopyFrom(base);

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
    VERBOSE(VB_IMPORTANT, "CreateCopy called on screentype - bad.");
}

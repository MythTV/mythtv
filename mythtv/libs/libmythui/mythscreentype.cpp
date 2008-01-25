#include <cassert>
#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "mythscreentype.h"
#include "mythscreenstack.h"
#include "mythmainwindow.h"

MythScreenType::MythScreenType(MythScreenStack *parent, const char *name,
                               bool fullscreen)
              : MythUIType(parent, name)
{
    assert(parent);

    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = parent;
    m_IsDeleting = false;

    // Can be overridden, of course, but default to full sized.
    m_Area = GetMythMainWindow()->GetUIScreenRect();
}

MythScreenType::MythScreenType(MythUIType *parent, const char *name,
                               bool fullscreen)
              : MythUIType(parent, name)
{
    m_FullScreen = fullscreen;
    m_CurrentFocusWidget = NULL;

    m_ScreenStack = NULL;
    m_IsDeleting = false;

    m_Area = GetMythMainWindow()->GetUIScreenRect();
}

MythScreenType::~MythScreenType()
{
}

bool MythScreenType::IsFullscreen(void)
{
    return m_FullScreen;
}

void MythScreenType::SetFullscreen(bool full)
{
    m_FullScreen = full;
}

MythUIType *MythScreenType::GetFocusWidget(void)
{
    return m_CurrentFocusWidget;
}

bool MythScreenType::SetFocusWidget(MythUIType *widget)
{
    if (!widget)
    {
        QPtrListIterator<MythUIType> it(m_FocusWidgetList);
        MythUIType *current;

        while ((current = it.current()))
        {
            if (current->CanTakeFocus())
            {
                widget = current;
                break;
            }
            ++it;
        }
    }

    if (!widget)
        return false;

    if (m_CurrentFocusWidget)
        m_CurrentFocusWidget->LoseFocus();
    m_CurrentFocusWidget = widget;
    m_CurrentFocusWidget->TakeFocus();

    return true;
}
 
bool MythScreenType::NextPrevWidgetFocus(bool up)
{
    if (!m_CurrentFocusWidget || m_FocusWidgetList.isEmpty())
        return SetFocusWidget(NULL);

    bool reachedCurrent = false;
    // QPtrListIterator will always start at the beginning of the list
    // in this function. It is not a traditional list iterator
    QPtrListIterator<MythUIType> it(m_FocusWidgetList);
    MythUIType *current;

    // There is probably a more efficient way to do this, but the list
    // is never going to be that big so it will do for now
    if (up)
    {
        while ((current = it.current()))
        {
            if (reachedCurrent)
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            ++it;
        }
    }
    else
    {
        it.toLast();
        while ((current = it.current()))
        {
            if (reachedCurrent)
                return SetFocusWidget(current);

            if (current == m_CurrentFocusWidget)
                reachedCurrent = true;

            --it;
        }
    }

    // fall back to first/last possible widget
    if (up)
        return SetFocusWidget(it.toFirst());
    else
        return SetFocusWidget(it.toLast());

    return false;
}

bool MythScreenType::BuildFocusList(void)
{
    m_FocusWidgetList.clear();

    AddFocusableChildrenToList(m_FocusWidgetList);

    if (m_FocusWidgetList.count() > 0)
        return true;

    return false;
}

MythScreenStack *MythScreenType::GetScreenStack(void)
{
    return m_ScreenStack;
}

void MythScreenType::aboutToHide(void)
{
}

void MythScreenType::aboutToShow(void)
{
}

bool MythScreenType::IsDeleting(void)
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

bool MythScreenType::ParseElement(QDomElement &element)
{
    if (element.tagName() == "area")
        m_Area = parseRect(element);
    else
        return false;
    
    return true;
}

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
    SetFocusWidget(NULL);    
};

void MythScreenType::CreateCopy(MythUIType *)
{
    VERBOSE(VB_IMPORTANT, "CreateCopy called on screentype - bad.");
}


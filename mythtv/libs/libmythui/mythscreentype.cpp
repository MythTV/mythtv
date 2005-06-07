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
        }
    }

    if (!widget)
        return false;

    if (m_CurrentFocusWidget)
        m_CurrentFocusWidget->LoseFocus();
    m_CurrentFocusWidget->TakeFocus();

    return true;
}
 
bool MythScreenType::NextPrevWidgetFocus(bool up)
{
    if (!m_CurrentFocusWidget)
        return SetFocusWidget(NULL);

    bool reachedCurrent = false;
    QPtrListIterator<MythUIType> it(m_FocusWidgetList);
    MythUIType *current;

    while ((current = it.current()))
    {
        if (reachedCurrent && current->CanTakeFocus())
            return SetFocusWidget(current);

        if (current == m_CurrentFocusWidget)
            reachedCurrent = true;

        if (up)
            ++it;
        else
           --it;
    }

    // fall back to first possible widget
    if (up)
        return SetFocusWidget(NULL);

    // fall back to last possible widget
    if (reachedCurrent)
    {
        it.toLast();
        while ((current = it.current()))
        {
            if (current->CanTakeFocus())
                return SetFocusWidget(current);
            --it;
        }
    }

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

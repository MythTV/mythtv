#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "mythpainter.h"

#include <iostream>
#include <cassert>
using namespace std;

#include <qobjectlist.h>
#include <qapplication.h>

const int kFadeVal = 10;

MythScreenStack::MythScreenStack(MythMainWindow *parent, const char *name, 
                                 bool mainstack)
               : QObject(parent, name)
{
    assert(parent);

    parent->AddScreenStack(this, mainstack);

    newTop = NULL;
    topScreen = NULL;

    m_DoTransitions = (GetMythPainter()->SupportsAlpha() && 
                       GetMythPainter()->SupportsAnimation());
    m_InNewTransition = false;
}

MythScreenStack::~MythScreenStack()
{
}

int MythScreenStack::TotalScreens(void)
{
    return m_Children.count();
}

void MythScreenStack::AddScreen(MythScreenType *screen, bool allowFade)
{
    if (!screen)
        return;

    qApp->lock();

    MythScreenType *old = topScreen; 
    if (old)
        old->aboutToHide();

    m_Children.push_back(screen);

    if (allowFade && m_DoTransitions)
    {
        newTop = screen;
        DoNewFadeTransition();
    }
    else
    {
        reinterpret_cast<MythMainWindow *>(parent())->update();
        RecalculateDrawOrder();
    }

    screen->aboutToShow();

    topScreen = screen;

    qApp->unlock();
}

void MythScreenStack::PopScreen(void)
{
    if (m_Children.isEmpty())
        return;

    MythScreenType *top = topScreen;

    if (!top || top->IsDeleting())
        return;

    MythMainWindow *mainwindow = GetMythMainWindow();

    qApp->lock();

    removeChild(top);
    if (m_DoTransitions && !mainwindow->IsExitingToMain())
    {
        top->SetFullscreen(false);
        top->SetDeleting(true);
        m_ToDelete.push_back(top);
        top->AdjustAlpha(1, -kFadeVal);
    }
    else
    {
        m_Children.pop_back();
        delete top;
        top = NULL;

        mainwindow->update();
        if (mainwindow->IsExitingToMain())
            QApplication::postEvent(mainwindow, new ExitToMainMenuEvent());
    }

    topScreen = NULL;

    RecalculateDrawOrder();

    // If we're fading it, we still want to draw it.
    if (top)
        m_DrawOrder.push_back(top);

    if (!m_Children.isEmpty())
    {
        QValueVector<MythScreenType *>::Iterator it;
        for (it = m_DrawOrder.begin(); it != m_DrawOrder.end(); ++it)
        {
            if (*it != top && !(*it)->IsDeleting())
            {
                topScreen = (*it);
                if (m_DoTransitions)
                {
                    (*it)->SetAlpha(0);
                    (*it)->AdjustAlpha(1, kFadeVal);
                }
                (*it)->aboutToShow();
            }
        }
    }

    if (topScreen)
        topScreen->SetRedraw();

    qApp->unlock();
}

MythScreenType *MythScreenStack::GetTopScreen(void)
{
    if (topScreen)
        return topScreen;
    if (!m_DrawOrder.isEmpty())
        return m_DrawOrder.back();
    return NULL;
}

void MythScreenStack::GetDrawOrder(QValueVector<MythScreenType *> &screens)
{
    if (m_InNewTransition)
        CheckNewFadeTransition();
    CheckDeletes();

    screens = m_DrawOrder;
}

void MythScreenStack::RecalculateDrawOrder(void)
{
    m_DrawOrder.clear();

    if (m_Children.isEmpty())
        return;

    QValueVector<MythScreenType *>::Iterator it;

    for (it = m_Children.begin(); it != m_Children.end(); ++it)
    {
        MythScreenType *screen = (*it);

        if (screen->IsFullscreen())
        {
            m_DrawOrder.clear();
            m_DrawOrder.push_back(screen);
        }
    }

    if (m_DrawOrder.isEmpty())
    {
        MythScreenType *screen = GetTopScreen();
        if (screen)
            m_DrawOrder.push_back(screen);
    }
}

void MythScreenStack::DoNewFadeTransition(void)
{
    m_InNewTransition = true;
    newTop->SetAlpha(0);
    newTop->AdjustAlpha(1, kFadeVal);

    if (newTop->IsFullscreen())
    {
        QValueVector<MythScreenType *>::Iterator it;
        for (it = m_DrawOrder.begin(); it != m_DrawOrder.end(); ++it)
        {
            if (!(*it)->IsDeleting())
                (*it)->AdjustAlpha(1, -kFadeVal);
        }

        m_DrawOrder.push_back(newTop);
    }
    else
        RecalculateDrawOrder();
}
 
void MythScreenStack::CheckNewFadeTransition(void)
{
    if (!newTop)
    {
        m_InNewTransition = false;
        return;
    }

    if (newTop->GetAlpha() >= 255)
    {
        m_InNewTransition = false;
        newTop = NULL;

        RecalculateDrawOrder();
    }
}

void MythScreenStack::CheckDeletes(void)
{
    if (m_ToDelete.isEmpty())
        return;

    bool changed = false;

    QValueVector<MythScreenType *>::Iterator it = m_ToDelete.begin();
    while (it != m_ToDelete.end() && !m_ToDelete.isEmpty())
    {
        bool deleteit = false;

        if ((*it)->GetAlpha() <= 0)
        {
            deleteit = true;
        }

        if (!deleteit)
        {
            bool found = false;

            QValueVector<MythScreenType *>::Iterator test;
            for (test = m_DrawOrder.begin(); test != m_DrawOrder.end(); ++test)
            {
                if (*it == *test)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                deleteit = true;
        }

        if (deleteit)
        {
            QValueVector<MythScreenType *>::Iterator test;
            for (test = m_Children.begin(); test != m_Children.end(); ++test)
            {
                if (*test == *it)
                {
                    m_Children.erase(test);
                    break;
                }
            }

            if (*it == newTop)
                newTop = NULL;
            delete (*it);
            m_ToDelete.erase(it);
            it = m_ToDelete.begin();
            changed = true;
            continue;
        }

        ++it;
    }

    if (changed)
    {
        RecalculateDrawOrder();
    }
}

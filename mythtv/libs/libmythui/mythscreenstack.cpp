#include "libmythbase/mythevent.h"
#include "libmythbase/mythcorecontext.h"

#include "mythscreenstack.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "mythpainter.h"

#include <chrono>

#include <QCoreApplication>
#include <QString>
#include <QTimer>

const int kFadeVal = 20;

MythScreenStack::MythScreenStack(MythMainWindow *parent, const QString &name,
                                 bool mainstack)
               : QObject(parent)
{
    setObjectName(name);

    if (parent)
        parent->AddScreenStack(this, mainstack);

    EnableEffects();
}

MythScreenStack::~MythScreenStack()
{
    CheckDeletes(true);

    while (!m_children.isEmpty())
    {
        MythScreenType *child = m_children.back();
        MythScreenStack::PopScreen(child, false, true); // Don't fade, do delete
    }
}

void MythScreenStack::EnableEffects(void)
{
    m_doTransitions = GetMythDB()->GetBoolSetting("SmoothTransitions", true) &&
                      GetPainter()->SupportsAlpha() &&
                      GetPainter()->SupportsAnimation();
}

int MythScreenStack::TotalScreens(void) const
{
    return m_children.count();
}

void MythScreenStack::AddScreen(MythScreenType *screen, bool allowFade)
{
    if (!screen)
        return;

    m_doInit = false;

    MythScreenType *old = m_topScreen;
    if (old && screen->IsFullscreen())
        old->aboutToHide();

    m_children.push_back(screen);

    if (allowFade && m_doTransitions)
    {
        m_newTop = screen;
        DoNewFadeTransition();
    }
    else
    {
        if (parent())
            reinterpret_cast<MythMainWindow *>(parent())->update();
        RecalculateDrawOrder();
        if (!screen->IsInitialized())
            m_doInit = true;
    }

    screen->aboutToShow();

    m_topScreen = screen;

    emit topScreenChanged(m_topScreen);
}

void MythScreenStack::PopScreen(MythScreenType *screen, bool allowFade,
                                bool deleteScreen)
{
    if (!screen)
    {
        screen = m_topScreen;
    }
    if (!screen || screen->IsDeleting())
        return;

    bool poppedFullscreen = screen->IsFullscreen();

    screen->aboutToHide();

    if (m_children.isEmpty())
        return;

    MythMainWindow *mainwindow = GetMythMainWindow();

    screen->setParent(nullptr);
    if ((screen == m_topScreen) && allowFade && m_doTransitions
        && !mainwindow->IsExitingToMain())
    {
        screen->SetFullscreen(false);
        if (deleteScreen)
        {
            screen->SetDeleting(true);
            m_toDelete.push_back(screen);
        }
        screen->AdjustAlpha(1, -kFadeVal);
    }
    else
    {
        for (int i = 0; i < m_children.size(); ++i)
        {
            if (m_children.at(i) == screen)
                m_children.remove(i);
        }
        if (deleteScreen)
            screen->deleteLater();

        screen = nullptr;

        mainwindow->update();
        if (mainwindow->IsExitingToMain())
        {
            QCoreApplication::postEvent(
                mainwindow, new QEvent(MythEvent::kExitToMainMenuEventType));
        }
    }

    m_topScreen = nullptr;

    MythScreenStack::RecalculateDrawOrder();

    // If we're fading it, we still want to draw it.
    if (screen && !m_drawOrder.contains(screen))
        m_drawOrder.push_back(screen);

    if (!m_children.isEmpty())
    {
        for (auto *draw : std::as_const(m_drawOrder))
        {
            if (draw != screen && !draw->IsDeleting())
            {
                m_topScreen = draw;
                draw->SetAlpha(255);
                if (poppedFullscreen)
                    draw->aboutToShow();
            }
        }
    }

    if (m_topScreen)
    {
        m_topScreen->SetRedraw();

        if (!allowFade || !m_doTransitions)
            emit topScreenChanged(m_topScreen);
    }
    else
    {
        // Screen still needs to be redrawn if we have popped the last screen
        // off the popup stack, or similar
        if (mainwindow->GetMainStack())
        {
            MythScreenType *mainscreen = mainwindow->GetMainStack()->GetTopScreen();
            if (mainscreen)
                mainscreen->SetRedraw();
        }

        if (!allowFade || !m_doTransitions)
            emit topScreenChanged(nullptr);
    }
}

MythScreenType *MythScreenStack::GetTopScreen(void) const
{
    if (m_topScreen)
        return m_topScreen;
    if (!m_drawOrder.isEmpty())
        return m_drawOrder.back();
    return nullptr;
}

void MythScreenStack::GetDrawOrder(QVector<MythScreenType *> &screens)
{
    if (m_inNewTransition)
        CheckNewFadeTransition();
    CheckDeletes();

    screens = m_drawOrder;
}

void MythScreenStack::GetScreenList(QVector<MythScreenType *> &screens)
{
    if (m_inNewTransition)
        CheckNewFadeTransition();
    CheckDeletes();

    screens = m_children;
}

void MythScreenStack::ScheduleInitIfNeeded(void)
{
    // make sure Init() is called outside the paintEvent
    if (m_doInit && m_topScreen && !m_initTimerStarted &&
        !m_topScreen->IsLoading())
    {
        m_initTimerStarted = true;
        QTimer::singleShot(100ms, this, &MythScreenStack::doInit);
    }
}

void MythScreenStack::doInit(void)
{
    if (m_doInit && m_topScreen)
    {
        m_doInit = false;

        if (!m_topScreen->IsLoaded())
            m_topScreen->LoadInForeground();

        if (!m_topScreen->IsInitialized())
            m_topScreen->doInit();
    }
    m_initTimerStarted = false;
}

void MythScreenStack::RecalculateDrawOrder(void)
{
    m_drawOrder.clear();

    if (m_children.isEmpty())
        return;

    for (auto *screen : std::as_const(m_children))
    {
        if (screen->IsFullscreen())
            m_drawOrder.clear();

        m_drawOrder.push_back(screen);
    }

    if (m_drawOrder.isEmpty())
    {
        MythScreenType *screen = GetTopScreen();
        if (screen)
            m_drawOrder.push_back(screen);
    }
}

void MythScreenStack::DoNewFadeTransition(void)
{
    m_inNewTransition = true;
    m_newTop->SetAlpha(0);
    m_newTop->AdjustAlpha(1, kFadeVal);

    if (m_newTop->IsFullscreen())
    {
        for (auto *draw : std::as_const(m_drawOrder))
        {
            if (!draw->IsDeleting())
                draw->AdjustAlpha(1, -kFadeVal);
        }

        m_drawOrder.push_back(m_newTop);
    }
    else
    {
        RecalculateDrawOrder();
    }
}

void MythScreenStack::CheckNewFadeTransition(void)
{
    if (!m_newTop)
    {
        m_inNewTransition = false;
        return;
    }

    if (m_newTop->GetAlpha() >= 255)
    {
        m_inNewTransition = false;
        if (!m_newTop->IsInitialized())
            m_doInit = true;
        m_newTop = nullptr;

        RecalculateDrawOrder();
    }
}

void MythScreenStack::CheckDeletes(bool force)
{
    if (m_toDelete.isEmpty())
        return;

    bool changed = false;

    QVector<MythScreenType *>::Iterator it = m_toDelete.begin();
    while (it != m_toDelete.end() && !m_toDelete.isEmpty())
    {
        bool deleteit = false;

        if (force || (*it)->GetAlpha() <= 0)
        {
            deleteit = true;
        }

        if (!deleteit)
        {
            bool found = false;

            for (const auto *test : std::as_const(m_drawOrder))
            {
                if (*it == test)
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
            // NOLINTNEXTLINE(readability-qualified-auto) for Qt6
            for (auto test = m_children.begin();
                 test != m_children.end();
                 ++test)
            {
                if (*test == *it)
                {
                    m_children.erase(test);
                    break;
                }
            }

            if (*it == m_newTop)
                m_newTop = nullptr;
            delete (*it);
            it = m_toDelete.erase(it);
            changed = true;
            continue;
        }

        ++it;
    }

    if (changed)
    {
        RecalculateDrawOrder();
        emit topScreenChanged(GetTopScreen());
    }
}

QString MythScreenStack::GetLocation(bool fullPath) const
{
    if (fullPath)
    {
        QString path;
        for (auto *child : std::as_const(m_children))
        {
            if (!child->IsDeleting())
            {
                if (path.isEmpty())
                    path = child->objectName();
                else
                    path += '/' + child->objectName();
            }
        }
        return path;
    }

    if (m_topScreen)
        return m_topScreen->objectName();

    return {};
}

MythPainter* MythScreenStack::GetPainter(void)
{
    return GetMythPainter();
}

// Qt
#include <QTimer>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#endif

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuiprogressbar.h"

#include "tv_play_win.h"


TvPlayWindow::TvPlayWindow(MythScreenStack *parent, const char *name)
  : MythScreenType(parent, name)
{
    SetCanTakeFocus(true);
}

bool TvPlayWindow::Create()
{
    // Load the theme for this screen
    bool foundtheme = CopyWindowFromBase("videowindow", this);
    if (!foundtheme)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Cannot load screen videowindow from base.xml");
        return false;
    }

    m_progressBar = dynamic_cast<MythUIProgressBar*>(GetChild("progress"));
    if (m_progressBar)
    {
        m_progressBar->SetVisible(false);
        m_progressBar->SetStart(0);
        m_progressBar->SetTotal(100);
        m_progressBar->SetUsed(m_progress);
    }

    return true;
}

void TvPlayWindow::UpdateProgress(void)
{
    if (!m_progressBar)
        return;
    m_progress++;
    if (m_progress > 100)
        m_progress = 0;
    m_progressBar->SetUsed(m_progress);
    m_progressBar->SetVisible(true);
}

/** \brief Mouse click/movement handler, recieves mouse gesture events
 *         from event loop. Should not be used directly.
 *
 *  \param event Mouse event
 */
bool TvPlayWindow::gestureEvent(MythGestureEvent *event)
{
    bool handled = false;
#ifdef Q_OS_ANDROID
    switch (event->GetGesture())
    {
        case MythGestureEvent::Click:
            switch (event->GetButton())
            {
                case Qt::RightButton :
                    LOG(VB_GENERAL, LOG_NOTICE, "TV Play Window R Click");
                    handled = true;
                    break;
                case Qt::LeftButton :
                    {
                        LOG(VB_GENERAL, LOG_NOTICE, "TV Play Window L Click");
                        // send it on like the others to be handled by TV
                        QCoreApplication::postEvent(GetMythMainWindow(), new MythGestureEvent(*event));
                        handled = true;
                    }
                    break;
                default:
                    LOG(VB_GENERAL, LOG_NOTICE, "TV Play Window other Click");
                    handled = true;
                    break;
            }
            break;
        default:
            break;
    }
#else
    if (event->GetGesture() == MythGestureEvent::Click)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "TV Play Window Click");
        handled = true;
    }
#endif
    return handled;
}

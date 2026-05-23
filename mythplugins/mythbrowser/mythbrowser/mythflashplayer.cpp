#include <cstdlib>
#include <iostream>

// qt
#include <QApplication>
#include <QEvent>

// MythTV
#include <libmythbase/mythlogging.h>
#include <libmythtv/playgroup.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuihelper.h>
#include <libmythui/mythuiwebbrowser.h>

// mythbrowser
#include "mythflashplayer.h"
#include "webpage.h"

MythFlashPlayer::MythFlashPlayer(MythScreenStack *parent,
                         QStringList &urlList)
    : MythScreenType (parent, "mythflashplayer"),
      m_url(urlList[0])
{
    m_fftime       = PlayGroup::GetSetting("Default", "skipahead", 30);
    m_rewtime      = PlayGroup::GetSetting("Default", "skipback", 5);
    m_jumptime     = PlayGroup::GetSetting("Default", "jump", 10);
    QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
    GetMythMainWindow()->PauseIdleTimer(true);
    MythMainWindow::DisableScreensaver();
}


MythFlashPlayer::~MythFlashPlayer()
{
    QGuiApplication::restoreOverrideCursor();

    if (m_browser)
    {
        m_browser->disconnect();
        DeleteChild(m_browser);
        m_browser = nullptr;
    }
    GetMythMainWindow()->PauseIdleTimer(false);
    MythMainWindow::RestoreScreensaver();
}

bool MythFlashPlayer::Create(void)
{
    if (!m_browser)
        m_browser = new MythUIWebBrowser(this, "mythflashplayer");
    m_browser->SetArea(MythRect(GetMythMainWindow()->GetUIScreenRect()));
    m_browser->Init();
    m_browser->SetActive(true);
    m_browser->Show();

    BuildFocusList();

    SetFocusWidget(m_browser);

    m_url.replace("mythflash://", "http://");
    LOG(VB_GENERAL, LOG_INFO, QString("Opening %1").arg(m_url));
    m_browser->LoadPage(QUrl::fromEncoded(m_url.toLocal8Bit()));

    return true;
}

void MythFlashPlayer::runJavaScript(const QString& source)
{
    if (m_browser)
        m_browser->RunJavaScript(source);
}

bool MythFlashPlayer::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Playback", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "PAUSE")
            runJavaScript("play();");
        else if (action == "INFO")
            runJavaScript("info();");
        else if (action == "SEEKFFWD")
            runJavaScript(QString("seek(%1);").arg(m_fftime));
        else if (action == "SEEKRWND")
            runJavaScript(QString("seek(-%1);").arg(m_rewtime));
        else if (action == "CHANNELUP")
            runJavaScript(QString("seek(%1);").arg(m_jumptime * 60));
        else if (action == "CHANNELDOWN")
            runJavaScript(QString("seek(-%1);").arg(m_jumptime * 60));
        else if (action == "VOLUMEUP")
            runJavaScript("adjustVolume(2);");
        else if (action == "VOLUMEDOWN")
            runJavaScript("adjustVolume(-2);");
        else
            handled = false;

        if (handled)
            return true;
    }

    if (m_browser)
        handled = m_browser->keyPressEvent(event);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

#include "moc_mythflashplayer.cpp"

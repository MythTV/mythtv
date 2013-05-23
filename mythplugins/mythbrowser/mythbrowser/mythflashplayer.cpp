#include <stdlib.h>
#include <iostream>

// qt
#include <QApplication>
#include <QEvent>

// myth
#include <mythlogging.h>
#include <mythcontext.h>
#include <mythmainwindow.h>
#include <mythuiwebbrowser.h>
#include <playgroup.h>

// mythbrowser
#include "webpage.h"
#include "mythflashplayer.h"

using namespace std;

MythFlashPlayer::MythFlashPlayer(MythScreenStack *parent,
                         QStringList &urlList)
    : MythScreenType (parent, "mythflashplayer"),
      m_browser(NULL), m_url(urlList[0])
{
    m_fftime       = PlayGroup::GetSetting("Default", "skipahead", 30);
    m_rewtime      = PlayGroup::GetSetting("Default", "skipback", 5);
    m_jumptime     = PlayGroup::GetSetting("Default", "jump", 10);
    qApp->setOverrideCursor(QCursor(Qt::BlankCursor));
    GetMythMainWindow()->PauseIdleTimer(true);
}


MythFlashPlayer::~MythFlashPlayer()
{
    qApp->restoreOverrideCursor();

    if (m_browser)
    {
        m_browser->disconnect();
        DeleteChild(m_browser);
        m_browser = NULL;
    }
    GetMythMainWindow()->PauseIdleTimer(false);
}

bool MythFlashPlayer::Create(void)
{
    if (!m_browser)
        m_browser = new MythUIWebBrowser(this, "mythflashplayer");
    m_browser->SetArea(GetMythMainWindow()->GetUIScreenRect());
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

QVariant MythFlashPlayer::evaluateJavaScript(const QString& source)
{
    return (m_browser ? m_browser->evaluateJavaScript(source) : QVariant());
}

bool MythFlashPlayer::keyPressEvent(QKeyEvent *event)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("TV Playback", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "PAUSE")
            evaluateJavaScript("play();");
        else if (action == "INFO")
            evaluateJavaScript("info();");
        else if (action == "SEEKFFWD")
            evaluateJavaScript(QString("seek(%1);").arg(m_fftime));
        else if (action == "SEEKRWND")
            evaluateJavaScript(QString("seek(-%1);").arg(m_rewtime));
        else if (action == "CHANNELUP")
            evaluateJavaScript(QString("seek(%1);").arg(m_jumptime * 60));
        else if (action == "CHANNELDOWN")
            evaluateJavaScript(QString("seek(-%1);").arg(m_jumptime * 60));
        else if (action == "VOLUMEUP")
            evaluateJavaScript("adjustVolume(2);");
        else if (action == "VOLUMEDOWN")
            evaluateJavaScript("adjustVolume(-2);");
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

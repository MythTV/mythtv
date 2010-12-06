#include <stdlib.h>
#include <iostream>

// qt
#include <QApplication>
#include <QEvent>

// myth
#include <mythverbose.h>
#include <mythcontext.h>
#include <libmythui/mythmainwindow.h>
#include <mythuiwebbrowser.h>

// mythbrowser
#include "webpage.h"
#include "mythflashplayer.h"

using namespace std;

MythFlashPlayer::MythFlashPlayer(MythScreenStack *parent,
                         QStringList &urlList)
    : MythScreenType (parent, "mythflashplayer"),
      m_browser(NULL), m_url(urlList[0])
{
    qApp->setOverrideCursor(QCursor(Qt::BlankCursor));
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
}

bool MythFlashPlayer::Create(void)
{
    m_browser = new MythUIWebBrowser(this, "mythflashplayer");
    m_browser->SetArea(GetMythMainWindow()->GetUIScreenRect());
    m_browser->Init();
    m_browser->SetActive(true);
    m_browser->Show();

    BuildFocusList();

    SetFocusWidget(m_browser);

    m_url.replace("mythflash://", "http://");
    VERBOSE(VB_GENERAL, QString("Opening %1").arg(m_url));
    m_browser->LoadPage(QUrl::fromEncoded(m_url.toLocal8Bit()));

    return true;
}

QVariant MythFlashPlayer::evaluateJavaScript(const QString& source)
{
    return m_browser->evaluateJavaScript(source);
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
        else if (action == "SEEKFFWD")
            evaluateJavaScript("seek(10);");
        else if (action == "SEEKRWND")
            evaluateJavaScript("seek(-10);");
        else if (action == "CHANNELUP")
            evaluateJavaScript("seek(60);");
        else if (action == "CHANNELDOWN")
            evaluateJavaScript("seek(-60);");
        else
            handled = false;

        if (handled)
            return true;
    }

    handled = m_browser->keyPressEvent(event);

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

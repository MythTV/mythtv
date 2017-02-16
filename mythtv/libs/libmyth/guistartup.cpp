
#include "langsettings.h"

// qt
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>

// libmythbase
#include "mythcorecontext.h"
#include "mythstorage.h"
#include "mythdirs.h"
#include "mythlogging.h"
#include "mythlocale.h"
#include "mythtranslation.h"
#include "iso3166.h"
#include "iso639.h"
#include "mythtimer.h"

// libmythui
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythmainwindow.h"
#include "mythscreenstack.h"
#include "mythuistatetype.h"
#include "mythuiprogressbar.h"
#include "mythdialogbox.h"

#include "guistartup.h"

GUIStartup::GUIStartup(MythScreenStack *parent, QEventLoop *eventLoop)
                 :MythScreenType(parent, "GUIStartup"),
                  m_Exit(false),
                  m_Setup(false),
                  m_Retry(false),
                  m_Search(false),
                  m_dummyButton(0),
                  m_retryButton(0),
                  m_searchButton(0),
                  m_setupButton(0),
                  m_exitButton(0),
                  m_statusState(0),
                  m_progressBar(0),
                  m_progressTimer(0),
                  m_loop(eventLoop),
                  m_dlgLoop(this),
                  m_total(0)
{
}

GUIStartup::~GUIStartup()
{
    if (m_progressTimer)
        delete m_progressTimer;

}

bool GUIStartup::Create(void)
{
    if (!LoadWindowFromXML("config-ui.xml", "guistartup", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_dummyButton, "dummy", &err);
    if (!err)
        UIUtilE::Assign(this, m_retryButton, "retry", &err);
    if (!err)
        UIUtilE::Assign(this, m_searchButton, "search", &err);
    if (!err)
        UIUtilE::Assign(this, m_setupButton, "setup", &err);
    if (!err)
        UIUtilE::Assign(this, m_exitButton, "exit", &err);
    if (!err)
        UIUtilE::Assign(this, m_statusState, "statusstate", &err);
    if (!err)
        UIUtilE::Assign(this, m_progressBar, "progress", &err);
    if (!err)
        UIUtilE::Assign(this, m_messageState, "messagestate", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 "Cannot load screen 'guistartup'");
        return false;
    }

    connect(m_retryButton, SIGNAL(Clicked()), SLOT(Retry()));
    connect(m_searchButton, SIGNAL(Clicked()), SLOT(Search()));
    connect(m_setupButton, SIGNAL(Clicked()), SLOT(Setup()));
    connect(m_exitButton, SIGNAL(Clicked()), SLOT(Close()));

    BuildFocusList();

    return true;
}

bool GUIStartup::setStatusState(const QString &name)
{
    bool ret = m_statusState->DisplayState(name);
    m_Exit = false;
    m_Setup = false;
    m_Search = false;
    m_Retry = false;
    return ret;
}

bool GUIStartup::setMessageState(const QString &name)
{
    bool ret = m_messageState->DisplayState(name);
    m_Exit = false;
    m_Setup = false;
    m_Search = false;
    m_Retry = false;
    return ret;
}


void GUIStartup::setTotal(int total)
{
    if (m_progressTimer)
        delete m_progressTimer;
    m_progressTimer = new MythTimer(MythTimer::kStartRunning);
    m_total = total*1000;
    m_progressBar->SetTotal(m_total);
    SetFocusWidget(m_dummyButton);

    m_Exit = false;
    m_Setup = false;
    m_Search = false;
    m_Retry = false;
}

// return = true if time is up
bool GUIStartup::updateProgress(bool finished)
{
    if (m_progressTimer) {
        int elapsed;
        if (finished)
            elapsed = m_total;
        else
            elapsed = m_progressTimer->elapsed();
        m_progressBar->SetUsed(elapsed);
        return elapsed >= m_total;
    }
    return false;
}

void GUIStartup::Close(void)
{
    QString message = tr("Do you really want to exit MythTV?");
    MythScreenStack *popupStack
      = GetMythMainWindow()->GetStack("popup stack");
    MythConfirmationDialog *confirmdialog
      = new MythConfirmationDialog(
         popupStack, message);

    if (confirmdialog->Create())
        popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, SIGNAL(haveResult(bool)),
            SLOT(OnClosePromptReturn(bool)));

    m_dlgLoop.exec();
}

void GUIStartup::OnClosePromptReturn(bool submit)
{
    if (m_dlgLoop.isRunning())
        m_dlgLoop.exit();

    if (submit)
    {
    if (m_loop->isRunning())
        m_loop->exit();
        m_Exit = true;
        MythScreenType::Close();
    }
}

void GUIStartup::Retry(void)
{
    m_Retry = true;
    if (m_loop->isRunning())
        m_loop->exit();
}

void GUIStartup::Search(void)
{
    m_Search = true;
    if (m_loop->isRunning())
        m_loop->exit();
}

void GUIStartup::Setup(void)
{
    m_Setup = true;
    if (m_loop->isRunning())
        m_loop->exit();
}


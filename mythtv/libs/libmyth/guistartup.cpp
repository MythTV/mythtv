//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////


#include "langsettings.h"

// C++
#include <chrono>

// qt
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>

// libmythbase
#include "libmythbase/iso3166.h"
#include "libmythbase/iso639.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlocale.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythstorage.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/mythtranslation.h"

// libmythui
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreenstack.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuibuttonlist.h"
#include "libmythui/mythuiprogressbar.h"
#include "libmythui/mythuistatetype.h"

#include "guistartup.h"

GUIStartup::GUIStartup(MythScreenStack *parent, QEventLoop *eventLoop)
                 :MythScreenType(parent, "GUIStartup"),
                  m_loop(eventLoop),
                  m_dlgLoop(this)
{
}

GUIStartup::~GUIStartup()
{
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

    connect(m_retryButton, &MythUIButton::Clicked, this, &GUIStartup::Retry);
    connect(m_searchButton, &MythUIButton::Clicked, this, &GUIStartup::Search);
    connect(m_setupButton, &MythUIButton::Clicked, this, &GUIStartup::Setup);
    connect(m_exitButton, &MythUIButton::Clicked, this, &GUIStartup::Close);
    connect(&m_timer, &QTimer::timeout, this, qOverload<>(&GUIStartup::updateProgress));

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


void GUIStartup::setTotal(std::chrono::seconds total)
{
    delete m_progressTimer;
    m_progressTimer = new MythTimer(MythTimer::kStartRunning);
    m_timer.start(500ms);
    m_total = total;
    m_progressBar->SetTotal(m_total.count());
    SetFocusWidget(m_dummyButton);

    m_Exit = false;
    m_Setup = false;
    m_Search = false;
    m_Retry = false;
}

// return = true if time is up
bool GUIStartup::updateProgress(bool finished)
{
    if (m_progressTimer)
    {
        std::chrono::milliseconds elapsed { 0ms };
        if (finished)
            elapsed = m_total;
        else
            elapsed = m_progressTimer->elapsed();
        m_progressBar->SetUsed(elapsed.count());
        if (elapsed >= m_total)
        {
            m_timer.stop();
            emit cancelPortCheck();
            delete m_progressTimer;
            m_progressTimer = nullptr;
        }
        return elapsed >= m_total;
    }
    m_timer.stop();
    return false;
}

void GUIStartup::updateProgress(void)
{
    updateProgress(false);
}

void GUIStartup::Close(void)
{
    std::chrono::milliseconds elapsed { 0ms };
    if (m_progressTimer)
    {
        elapsed = m_progressTimer->elapsed();
        m_progressTimer->stop();
        m_timer.stop();
    }
    QString message = tr("Do you really want to exit MythTV?");
    MythScreenStack *popupStack
      = GetMythMainWindow()->GetStack("popup stack");
    auto *confirmdialog = new MythConfirmationDialog(popupStack, message);

    if (confirmdialog->Create())
        popupStack->AddScreen(confirmdialog);

    connect(confirmdialog, &MythConfirmationDialog::haveResult,
            this, &GUIStartup::OnClosePromptReturn);

    m_dlgLoop.exec();

    if (m_progressTimer && !m_Exit)
    {
        m_progressTimer->start();
        m_progressTimer->addMSecs(elapsed);
        m_timer.start();
    }
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
        emit cancelPortCheck();
        MythScreenType::Close();
    }
}

void GUIStartup::Retry(void)
{
    m_Retry = true;
    emit cancelPortCheck();
    if (m_loop->isRunning())
        m_loop->exit();
}

void GUIStartup::Search(void)
{
    m_Search = true;
    emit cancelPortCheck();
    if (m_loop->isRunning())
        m_loop->exit();
}

void GUIStartup::Setup(void)
{
    m_Setup = true;
    emit cancelPortCheck();
    if (m_loop->isRunning())
        m_loop->exit();
}


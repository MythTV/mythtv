/*
 *  Class MythTerminal
 *
 *  Copyright (C) Daniel Kristjansson 2008
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <inttypes.h>

// MythTV headers
#include "mythlogging.h"
#include "mythterminal.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuitextedit.h"

MythTerminal::MythTerminal(MythScreenStack *parent, QString _program,
                           QStringList _arguments) :
    MythScreenType(parent, "terminal"), m_lock(QMutex::Recursive),
    m_running(false), m_process(new QProcess()), m_program(_program),
    m_arguments(_arguments), m_currentLine(NULL), m_output(NULL),
    m_textEdit(NULL), m_enterButton(NULL)
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, SIGNAL(readyRead()),
            this,      SLOT(ProcessHasText()));

    connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this,      SLOT(  ProcessFinished(int, QProcess::ExitStatus)));
}

void MythTerminal::TeardownAll(void)
{
    if (m_process)
    {
        QMutexLocker locker(&m_lock);
        if (m_process)
        {
            if (m_running)
                Kill();
            m_process->disconnect();
            m_process->deleteLater();
            m_process = NULL;
        }
    }
}

void MythTerminal::AddText(const QString &_str)
{
    QMutexLocker locker(&m_lock);
    QString str = _str;
    while (str.length())
    {
        int nlf = str.indexOf("\r\n");
        nlf = (nlf < 0) ? str.indexOf("\r") : nlf;
        nlf = (nlf < 0) ? str.indexOf("\n") : nlf;

        QString curStr = (nlf >= 0) ? str.left(nlf) : str;
        if (curStr.length())
        {
            if (!m_currentLine)
                m_currentLine = new MythUIButtonListItem(m_output, curStr);
            else
                m_currentLine->SetText(m_currentLine->GetText() + curStr);
        }

        if (nlf >= 0)
        {
            m_currentLine = new MythUIButtonListItem(m_output, QString());
            m_output->SetItemCurrent(m_currentLine);
            str = str.mid(nlf + 1);
        }
        else
        {
            str = "";
        }
    }
}

void MythTerminal::Start(void)
{
    QMutexLocker locker(&m_lock);
    m_process->start(m_program, m_arguments);
    m_running = true;
}

void MythTerminal::Kill(void)
{
    QMutexLocker locker(&m_lock);
    m_process->kill();
    m_running = false;
}

bool MythTerminal::IsDone(void) const
{
    QMutexLocker locker(&m_lock);
    return QProcess::NotRunning == m_process->state();
}

void MythTerminal::ProcessHasText(void)
{
    QMutexLocker locker(&m_lock);
    int64_t len = m_process->bytesAvailable();

    if (len <= 0)
        return;

    QByteArray buf = m_process->read(len);
    AddText(QString(buf));
}

void MythTerminal::Init(void)
{
    Start();
}

void MythTerminal::ProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus)
{
    QMutexLocker locker(&m_lock);
    if (exitStatus == QProcess::CrashExit) {
        AddText(tr("*** Crashed with status: %1 ***").arg(exitCode));
    } else {
        AddText(tr("*** Exited with status: %1 ***").arg(exitCode));
    }
    m_running = false;
    m_enterButton->SetEnabled(false);
    m_textEdit->SetEnabled(false);
}

bool MythTerminal::Create(void)
{
    if (!LoadWindowFromXML("standardsetting-ui.xml", "terminal", this))
        return false;

    bool error = false;
    UIUtilE::Assign(this, m_output, "output", &error);
    UIUtilE::Assign(this, m_textEdit, "textedit", &error);
    UIUtilE::Assign(this, m_enterButton, "enter", &error);

    if (error)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme elements missing.");
        return false;
    }

    BuildFocusList();

    MythUIButton *close = NULL;
    UIUtilW::Assign(this, close, "close");
    if (close)
        connect(close, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_enterButton, &MythUIButton::Clicked,
            this,
            [this]()
            {
                QMutexLocker locker(&m_lock);
                if (m_running)
                {
                    QString text = m_textEdit->GetText() + "\r\n";
                    AddText(text);

                    m_process->write(text.toLocal8Bit().constData());
                }
            });

    return true;
}

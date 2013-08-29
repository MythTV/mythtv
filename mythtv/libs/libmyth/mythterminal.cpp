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
#include "mythterminal.h"

MythTerminal::MythTerminal(QString _program, QStringList _arguments) :
    lock(QMutex::Recursive), running(false),
    process(new QProcess()), program(_program), arguments(_arguments),
    curLabel(""), curValue(0), filter(new MythTerminalKeyFilter())
{
    addSelection(curLabel, QString::number(curValue));

    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, SIGNAL(readyRead()),
            this,    SLOT(  ProcessHasText()));

    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)),
            this,    SLOT(  ProcessFinished(int, QProcess::ExitStatus)));

    connect(filter,  SIGNAL(KeyPressd(QKeyEvent*)),
            this,    SLOT(  ProcessSendKeyPress(QKeyEvent*)));
    SetEventFilter(filter);
}

void MythTerminal::TeardownAll(void)
{
    if (process)
    {
        QMutexLocker locker(&lock);
        if (running)
            Kill();
        process->disconnect();
    }

    if (filter)
    {
        filter->disconnect();
    }

    if (process)
    {
        process->deleteLater();
        process = NULL;
    }

    if (filter)
    {
        filter->deleteLater();
        filter = NULL;
    }
}

void MythTerminal::AddText(const QString &_str)
{
    QMutexLocker locker(&lock);
    QString str = _str;
    while (str.length())
    {
        int nlf = str.indexOf("\r\n");
        nlf = (nlf < 0) ? str.indexOf("\r") : nlf;
        nlf = (nlf < 0) ? str.indexOf("\n") : nlf;

        QString curStr = (nlf >= 0) ? str.left(nlf) : str;
        if (curStr.length())
        {
            curLabel += curStr;
            ReplaceLabel(curLabel, QString::number(curValue));
        }

        if (nlf >= 0)
        {
            addSelection(curLabel = "", QString::number(++curValue));
            str = str.mid(nlf + 1);
        }
        else
        {
            str = "";
        }
    }
    if (lbwidget)
    {
        lbwidget->setEnabled(true);
        lbwidget->setFocus();
        lbwidget->setCurrentRow(lbwidget->count() - 1);
    }
}

void MythTerminal::Start(void)
{
    QMutexLocker locker(&lock);
    process->start(program, arguments);
    running = true;
}

void MythTerminal::Kill(void)
{
    QMutexLocker locker(&lock);
    process->kill();
    running = false;
}

bool MythTerminal::IsDone(void) const
{
    QMutexLocker locker(&lock);
    return QProcess::NotRunning == process->state();
}

void MythTerminal::ProcessHasText(void)
{
    QMutexLocker locker(&lock);
    int64_t len = process->bytesAvailable();

    if (len <= 0)
        return;

    QByteArray buf = process->read(len);
    AddText(QString(buf));
}

void MythTerminal::ProcessSendKeyPress(QKeyEvent *e)
{
    QMutexLocker locker(&lock);
    if (running && process && e->text().length())
    {
        QByteArray text = e->text().toLocal8Bit();
        AddText(text.constData());
        if (e->text()=="\n" || e->text()=="\r")
            process->write("\r\n");
        else
            process->write(text.constData());
    }
}

void MythTerminal::ProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus)
{
    QMutexLocker locker(&lock);
    AddText(tr("*** Exited with status: %1 ***").arg(exitCode));
    setEnabled(false);
    running = false;
}

////////////////////////////////////////////////////////////////////////

bool MythTerminalKeyFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *e = (QKeyEvent*)(event);
        QStringList actions;
        bool handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions,
                                                              false);
        if (!handled && !actions.isEmpty())
        {
            if (actions.contains("LEFT") || actions.contains("RIGHT") ||
                actions.contains("UP") || actions.contains("DOWN") ||
                actions.contains("ESCAPE"))
            {
                return QObject::eventFilter(obj, event);
            }
            else
            {
                emit KeyPressd(e);
                e->accept();
                return true;
            }
        }
        else
        {
            emit KeyPressd(e);
            e->accept();
            return true;
        }
    }
    else
    {
        return QObject::eventFilter(obj, event);
    }
}


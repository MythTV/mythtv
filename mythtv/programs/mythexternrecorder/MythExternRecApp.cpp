/*  -*- Mode: c++ -*-
 *
 * Copyright (C) John Poet 2018
 *
 * This file is part of MythTV
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// C/C++
#include <csignal>
#include <thread>
#include <unistd.h>

// Qt
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>

// MythTV
#include "libmythbase/mythchrono.h"

// MythExternRecorder
#include "MythExternRecApp.h"
#include "mythexternrecorder_commandlineparser.h"

#define LOC Desc()

MythExternRecApp::MythExternRecApp(QString command,
                                   QString conf_file,
                                   QString log_file,
                                   QString logging)
    : m_recCommand(std::move(command))
    , m_logFile(std::move(log_file))
    , m_logging(std::move(logging))
    , m_configIni(std::move(conf_file))
{
    if (m_configIni.isEmpty() || !config())
        m_recDesc = m_recCommand;

    m_command = m_recCommand;

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Channels in '%1', Tuner: '%2', Scanner: '%3'")
        .arg(m_channelsIni, m_tuneCommand, m_scanCommand));

    m_desc = m_recDesc;
    m_desc.replace("%URL%", "");
    m_desc.replace("%CHANNUM%", "");
    m_desc.replace("%CHANNAME%", "");
    m_desc.replace("%CALLSIGN%", "");
    ReplaceVariables(m_desc);
}

MythExternRecApp::~MythExternRecApp(void)
{
    Close();
}

/* Remove any non-replaced variables along with any dependant strings.
   Dependant strings are wrapped in {} */
QString MythExternRecApp::sanitize_var(const QString & var)
{
    qsizetype p1 { -1 };
    qsizetype p2 { -1 };
    QString cleaned = var;

    while ((p1 = cleaned.indexOf('{')) != -1)
    {
        p2 = cleaned.indexOf('}', p1);
        if (cleaned.mid(p1, p2 - p1).indexOf('%') == -1)
        {
            // Just remove the '{' and '}'
            cleaned = cleaned.remove(p2, 1);
            cleaned = cleaned.remove(p1, 1);
        }
        else
        {
            // Remove the contents of { ... }
            cleaned = cleaned.remove(p1, p2 - p1 + 1);
        }
    }

    LOG(VB_CHANNEL, LOG_DEBUG, QString("Sanitized: '%1' -> '%2'")
        .arg(var, cleaned));

    return cleaned;
}

QString MythExternRecApp::replace_extra_args(const QString & var,
                                             const QVariantMap & extra_args) const
{
    QString result = var;
    /*
      Replace any information provided in JSON message
     */
    for (auto it = extra_args.keyValueBegin();
         it != extra_args.keyValueEnd(); ++it)
    {
        if (it->first == "command")
            continue;
        result.replace(QString("\%%1\%").arg(it->first.toUpper()),
                        it->second.toString());
        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString("Replaced '%1' with '%2'")
            .arg(it->first.toUpper(), it->second.toString()));
    }
    result = sanitize_var(result);

    return result;
}

void MythExternRecApp::ReplaceVariables(QString & cmd) const
{
    QMap<QString, QString>::const_iterator Ivar;
    for (Ivar = m_settingVars.constBegin();
         Ivar != m_settingVars.constEnd(); ++Ivar)
    {
        QString repl = "%" + Ivar.key() + "%";
        if (cmd.indexOf(repl) >= 0)
        {
            LOG(VB_CHANNEL, LOG_DEBUG, QString("Replacing '%1' with '%2'")
                .arg(repl, Ivar.value()));
            cmd.replace(repl, Ivar.value());
        }
        else
        {
            LOG(VB_CHANNEL, LOG_DEBUG, QString("Did not find '%1' in '%2'")
                .arg(repl, cmd));
        }
    }
}

QString MythExternRecApp::Desc(void) const
{
    QString extra;

    if (m_proc.processId() > 0)
        extra = QString("(pid %1) ").arg(m_proc.processId());

    QString desc = m_desc;
    ReplaceVariables(desc);

    return QString("%1%2 ").arg(extra, desc);
}

bool MythExternRecApp::config(void)
{
    QFileInfo conf_info = QFileInfo(m_configIni);

    if (!QFileInfo::exists(m_configIni))
    {
        m_fatalMsg = QString("ERR:Config file '%1' does not exist "
                             "in '%2'")
                     .arg(conf_info.fileName(),
                          conf_info.absolutePath());
        LOG(VB_GENERAL, LOG_CRIT, m_fatalMsg);
        m_fatal = true;
        return false;
    }

    QSettings settings(m_configIni, QSettings::IniFormat);

    if (settings.childGroups().contains("VARIABLES"))
    {
        LOG(VB_CHANNEL, LOG_DEBUG, "Parsing variables");
        settings.beginGroup("VARIABLES");

        QStringList childKeys = settings.childKeys();
        for (const QString & var : std::as_const(childKeys))
        {
            m_settingVars[var] = settings.value(var).toString();
            LOG(VB_CHANNEL, LOG_INFO, QString("%1=%2")
                .arg(var, settings.value(var).toString()));
        }
        settings.endGroup();

        /* Replace defined VARs in the subsequently defined VARs */
        QMap<QString, QString>::iterator Ivar;
        QMap<QString, QString>::iterator Ivar2;
        for (Ivar = m_settingVars.begin();
             Ivar != m_settingVars.end(); ++Ivar)
        {
            QString repl = "%" + Ivar.key() + "%";
            Ivar2 = Ivar;
            for (++Ivar2; Ivar2 != m_settingVars.end(); ++Ivar2)
            {
                if ((*Ivar2).indexOf(repl) >= 0)
                {
                    LOG(VB_CHANNEL, LOG_DEBUG, QString("Replacing '%1' with '%2'")
                        .arg(repl, Ivar.value()));
                    (*Ivar2).replace(repl, Ivar.value());
                }
            }
        }
    }
    else
    {
        LOG(VB_CHANNEL, LOG_DEBUG, "No VARIABLES section");
    }

    if (!settings.contains("RECORDER/command"))
    {
        m_fatalMsg = QString("ERR:Config file %1 file missing "
                             "[RECORDER]/command")
                     .arg(conf_info.absolutePath());
        LOG(VB_GENERAL, LOG_CRIT, m_fatalMsg);
        m_fatal = true;
        return false;
    }

    m_recCommand  = settings.value("RECORDER/command").toString();
    m_recDesc     = settings.value("RECORDER/desc").toString();
    m_cleanup     = settings.value("RECORDER/cleanup").toString();
    m_tuneCommand = settings.value("TUNER/command", "").toString();
    m_newEpisodeCommand = settings.value("TUNER/newepisodecommand", "").toString();
    m_onDataStart = settings.value("TUNER/ondatastart", "").toString();
    m_channelsIni = settings.value("TUNER/channels", "").toString();
    m_lockTimeout = settings.value("TUNER/timeout", "").toInt();
    m_scanCommand = settings.value("SCANNER/command", "").toString();
    m_scanTimeout = settings.value("SCANNER/timeout", "").toInt();

    ReplaceVariables(m_recCommand);
    ReplaceVariables(m_recDesc);
    ReplaceVariables(m_cleanup);
    ReplaceVariables(m_tuneCommand);
    ReplaceVariables(m_newEpisodeCommand);
    ReplaceVariables(m_onDataStart);
    ReplaceVariables(m_scanCommand);

    settings.beginGroup("ENVIRONMENT");

    m_appEnv.clear();
    QStringList keys = settings.childKeys();
    QStringList::const_iterator Ienv;
    for (Ienv = keys.constBegin(); Ienv != keys.constEnd(); ++Ienv)
    {
        if (!(*Ienv).isEmpty() && (*Ienv)[0] != '#')
            m_appEnv.insert((*Ienv).toLocal8Bit().constData(),
                             settings.value(*Ienv).toString());
    }

    if (!m_channelsIni.isEmpty())
    {
        if (!QFileInfo::exists(m_channelsIni))
        {
            // Assume the channels config is in the same directory as
            // main config
            QDir chan_path = QFileInfo(m_configIni).absolutePath();
            QFileInfo ini(chan_path, m_channelsIni);
            m_channelsIni = ini.absoluteFilePath();
        }
    }

    return true;
}

bool MythExternRecApp::Open(void)
{
    if (m_fatal)
    {
        emit SendMessage("Open", "0", m_fatalMsg, "ERR");
        return false;
    }

    if (m_command.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": No recorder provided.");
        emit SendMessage("Open", "0", "No recorder provided.", "ERR");
        return false;
    }

    if (!m_appEnv.isEmpty())
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QMap<QString, QString>::const_iterator Ienv;
        for (Ienv = m_appEnv.constBegin();
             Ienv != m_appEnv.constEnd(); ++Ienv)
        {
            env.insert(Ienv.key(), Ienv.value());
            LOG(VB_RECORD, LOG_INFO, LOC + QString(" ENV: '%1' = '%2'")
                .arg(Ienv.key(), Ienv.value()));
        }
        m_proc.setProcessEnvironment(env);
    }

    QObject::connect(&m_proc, &QProcess::started, this,
                     &MythExternRecApp::ProcStarted);
#if 0
    QObject::connect(&m_proc, &QProcess::readyReadStandardOutput, this,
                     &MythExternRecApp::ProcReadStandardOutput);
#endif
    QObject::connect(&m_proc, &QProcess::readyReadStandardError, this,
                     &MythExternRecApp::ProcReadStandardError);

    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    QObject::connect(&m_proc, &QProcess::errorOccurred,
                     this, &MythExternRecApp::ProcError);

    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    QObject::connect(&m_proc,
                     static_cast<void (QProcess::*)
                     (int,QProcess::ExitStatus exitStatus)>
                     (&QProcess::finished),
                     this, &MythExternRecApp::ProcFinished);

    qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");
    QObject::connect(&m_proc, &QProcess::stateChanged, this,
                     &MythExternRecApp::ProcStateChanged);

    LOG(VB_RECORD, LOG_INFO, LOC + ": Opened");

    emit Opened();
    return true;
}

void MythExternRecApp::TerminateProcess(QProcess & proc, const QString & desc) const
{
    m_terminating = true;
    if (proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGTERM to %1(%2)").arg(desc).arg(proc.processId()));
        proc.terminate();
        proc.waitForFinished();
    }
    if (proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGKILL to %1(%2)").arg(desc).arg(proc.processId()));
        proc.kill();
        proc.waitForFinished();
    }
    m_terminating = false;
}

Q_SLOT void MythExternRecApp::Close(void)
{
    if (m_run)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + ": Closing application.");
        m_run = false;
        m_runCond.notify_all();
        std::this_thread::sleep_for(50us);
    }

    if (m_tuneProc.state() == QProcess::Running)
    {
        m_tuneProc.closeReadChannel(QProcess::StandardOutput);
        TerminateProcess(m_tuneProc, "App");
    }

    if (m_proc.state() == QProcess::Running)
    {
        m_proc.closeReadChannel(QProcess::StandardOutput);
        TerminateProcess(m_proc, "App");
        std::this_thread::sleep_for(50us);
    }

    emit Done();
}

void MythExternRecApp::Run(void)
{
    QByteArray buf;

    while (m_run)
    {
        {
            std::unique_lock<std::mutex> lk(m_runMutex);
            m_runCond.wait_for(lk, std::chrono::milliseconds(10));
        }

        if (m_proc.state() == QProcess::Running)
        {
            if (m_proc.waitForReadyRead(50))
            {
                buf = m_proc.read(m_blockSize);
                if (!buf.isEmpty())
                    emit Fill(buf);
            }
        }

        qApp->processEvents();
    }

    if (m_proc.state() == QProcess::Running)
    {
        m_proc.closeReadChannel(QProcess::StandardOutput);
        TerminateProcess(m_proc, "App");
    }

    emit Done();
}

Q_SLOT void MythExternRecApp::Cleanup(void)
{
    m_tunedChannel.clear();

    if (m_cleanup.isEmpty())
        return;

    QStringList args = MythCommandLineParser::MythSplitCommandString(m_cleanup);
    QString cmd = args.takeFirst();

    LOG(VB_RECORD, LOG_WARNING, LOC +
        QString(" Beginning cleanup: '%1'").arg(cmd));

    cmd = replace_extra_args(cmd, m_chaninfo);

    QProcess cleanup;
    cleanup.start(cmd, args);
    if (!cleanup.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Failed to start cleanup process: "
            + ENO);
        return;
    }
    cleanup.waitForFinished(5000);
    if (cleanup.state() == QProcess::NotRunning)
    {
        if (cleanup.exitStatus() != QProcess::NormalExit)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + ": Cleanup process failed: " + ENO);
            return;
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + ": Cleanup finished.");
}

Q_SLOT void MythExternRecApp::DataStarted(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "DataStarted");

    QString startcmd = m_onDataStart;

    if (!m_channelsIni.isEmpty())
    {
        QSettings settings(m_channelsIni, QSettings::IniFormat);
        settings.beginGroup(m_tunedChannel);

        QString cmd = settings.value("ONSTART").toString();
        if (!cmd.isEmpty())
        {
            ReplaceVariables(cmd);
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString(": Using ONSTART cmd from '%1': '%2'")
                .arg(m_channelsIni, cmd));
            startcmd = cmd;
        }

        settings.endGroup();
    }

    if (startcmd.isEmpty())
        return;
    startcmd = replace_extra_args(startcmd, m_chaninfo);

    bool background = false;
    int pos = startcmd.lastIndexOf(QChar('&'));
    if (pos > 0)
    {
        background = true;
        startcmd = startcmd.left(pos);
    }

    QStringList args = MythCommandLineParser::MythSplitCommandString(startcmd);
    startcmd = args.takeFirst();

    TerminateProcess(m_finishTuneProc, "FinishTuning");

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Finishing tune: '%1' %3")
        .arg(startcmd, background ? "in the background" : ""));

    m_finishTuneProc.start(startcmd, args);
    if (!m_finishTuneProc.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Failed to finish tune process: "
            + ENO);
        return;
    }

    if (!background)
    {
        m_finishTuneProc.waitForFinished(5000);
        if (m_finishTuneProc.state() == QProcess::NotRunning)
        {
            if (m_finishTuneProc.exitStatus() != QProcess::NormalExit)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + ": Finish tune failed: " + ENO);
                return;
            }
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + ": tunning finished.");
}

Q_SLOT void MythExternRecApp::LoadChannels(const QString & serial)
{
    if (m_channelsIni.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("LoadChannels", serial, "No channels configured.", "ERR");
        return;
    }

    if (!m_scanCommand.isEmpty())
    {
        QString cmd = m_scanCommand;
        cmd.replace("%CHANCONF%", m_channelsIni);

        QStringList args = MythCommandLineParser::MythSplitCommandString(cmd);
        cmd = args.takeFirst();

        QProcess scanner;
        scanner.start(cmd, args);

        if (!scanner.waitForStarted())
        {
            QString errmsg = QString("Failed to start '%1': ").arg(cmd) + ENO;
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("LoadChannels", serial, errmsg, "ERR");
            return;
        }

        QByteArray    buf;
        QElapsedTimer timer;

        timer.start();
        while (timer.elapsed() < m_scanTimeout)
        {
            if (scanner.waitForReadyRead(50))
            {
                buf = scanner.readLine();
                if (!buf.isEmpty())
                {
                    LOG(VB_RECORD, LOG_INFO, LOC + ": " + buf);
                    MythLog(buf);
                }
            }

            if (scanner.state() != QProcess::Running)
                break;

            if (scanner.waitForFinished(50 /* msecs */))
                break;
        }
        if (timer.elapsed() >= m_scanTimeout)
        {
            QString errmsg = QString("Timedout waiting for '%1'").arg(cmd);
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("LoadChannels", serial, errmsg, "ERR");
            return;
        }
    }

    if (m_chanSettings == nullptr)
        m_chanSettings = new QSettings(m_channelsIni, QSettings::IniFormat);
    m_chanSettings->sync();
    m_channels = m_chanSettings->childGroups();

    emit SendMessage("LoadChannels", serial, QString::number(m_channels.size()), "OK");
}

void MythExternRecApp::GetChannel(const QString & serial, const QString & func)
{
    if (m_channelsIni.isEmpty() || m_channels.empty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("FirstChannel", serial, "No channels configured.", "ERR");
        return;
    }

    if (m_chanSettings == nullptr)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": Invalid channel configuration.");
        emit SendMessage(func, serial, "Invalid channel configuration.", "ERR");
        return;
    }

    if (m_channels.size() <= m_channelIdx)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": No more channels.");
        emit SendMessage(func, serial, "No more channels", "ERR");
        return;
    }

    QString channum = m_channels[m_channelIdx++];

    m_chanSettings->beginGroup(channum);

    QString name     = m_chanSettings->value("NAME").toString();
    QString callsign = m_chanSettings->value("CALLSIGN").toString();
    QString xmltvid  = m_chanSettings->value("XMLTVID").toString();
    QString icon     = m_chanSettings->value("ICON").toString();

    m_chanSettings->endGroup();

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString(": NextChannel Name:'%1',Callsign:'%2',xmltvid:%3,Icon:%4")
        .arg(name, callsign, xmltvid, icon));

    emit SendMessage(func, serial, QString("%1,%2,%3,%4,%5")
                     .arg(channum, name, callsign,
                          xmltvid, icon), "OK");
}

Q_SLOT void MythExternRecApp::FirstChannel(const QString & serial)
{
    m_channelIdx = 0;
    GetChannel(serial, "FirstChannel");
}

Q_SLOT void MythExternRecApp::NextChannel(const QString & serial)
{
    GetChannel(serial, "NextChannel");
}

void MythExternRecApp::NewEpisodeStarting(void)
{
    QString cmd = m_newEpisodeCommand;
    int     pos = cmd.lastIndexOf(QChar('&'));
    bool    background = false;

    if (pos > 0)
    {
        background = true;
        cmd = cmd.left(pos);
    }

    cmd = replace_extra_args(cmd, m_chaninfo);

    QStringList args = MythCommandLineParser::MythSplitCommandString(cmd);
    cmd = args.takeFirst();

    LOG(VB_RECORD, LOG_WARNING, LOC +
        QString(" New episode starting on current channel: '%1'").arg(cmd));

    if (m_tuneProc.state() == QProcess::Running)
        TerminateProcess(m_tuneProc, "Tune");

    m_tuneProc.start(cmd, args);
    if (!m_tuneProc.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            " NewEpisodeStarting: Failed to start process: " + ENO);
        return;
    }
    if (background)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            "NewEpisodeStarting: running in background.");
        return;
    }

    m_tuneProc.waitForFinished(5000);
    if (m_tuneProc.state() == QProcess::NotRunning)
    {
        if (m_tuneProc.exitStatus() != QProcess::NormalExit)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                " NewEpisodeStarting: process failed: " + ENO);
            return;
        }
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "NewEpisodeStarting: finished.");
}

Q_SLOT void MythExternRecApp::TuneChannel(const QString & serial,
                                          const QVariantMap & chaninfo)
{
    m_chaninfo = chaninfo;

    if (m_tuneCommand.isEmpty() && m_channelsIni.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No 'tuner' configured.");
        emit SendMessage("TuneChannel", serial, "No 'tuner' configured.", "ERR");
        return;
    }

    QString channum = m_chaninfo["channum"].toString();

    if (m_tunedChannel == channum)
    {
        if (!m_newEpisodeCommand.isEmpty())
            NewEpisodeStarting();

        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("TuneChannel: Already on %1").arg(channum));
        emit SendMessage("TuneChannel", serial,
                         QString("Tunned to %1").arg(channum), "OK");
        return;
    }

    m_desc    = m_recDesc;
    m_command = m_recCommand;

    QString tunecmd = m_tuneCommand;
    QString url;
    bool    background = false;

    if (!m_channelsIni.isEmpty())
    {
        QSettings settings(m_channelsIni, QSettings::IniFormat);
        settings.beginGroup(channum);

        QString cmd = settings.value("TUNE").toString();
        if (!cmd.isEmpty())
        {
            ReplaceVariables(cmd);
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString(": Using tune cmd from '%1': '%2'")
                .arg(m_channelsIni, cmd));
            tunecmd = cmd;
        }

        url = settings.value("URL").toString();
        if (!url.isEmpty())
        {
            if (tunecmd.indexOf("%URL%") >= 0)
            {
                tunecmd.replace("%URL%", url);
                LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                    QString(": '%URL%' replaced with '%1' in tunecmd: '%2'")
                    .arg(url, tunecmd));
            }

            if (m_command.indexOf("%URL%") >= 0)
            {
                m_command.replace("%URL%", url);
                LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                    QString(": '%URL%' replaced with '%1' in cmd: '%2'")
                    .arg(url, m_command));
            }
        }

        m_desc.replace("%CHANNAME%", settings.value("NAME").toString());
        m_desc.replace("%CALLSIGN%", settings.value("CALLSIGN").toString());

        settings.endGroup();
    }

    if (m_tuneProc.state() == QProcess::Running)
        TerminateProcess(m_tuneProc, "Tune");

    int pos = tunecmd.lastIndexOf(QChar('&'));
    if (pos > 0)
    {
        background = true;
        tunecmd = tunecmd.left(pos);
    }

    tunecmd = replace_extra_args(tunecmd, m_chaninfo);

    if (!m_logFile.isEmpty() && m_command.indexOf("%LOGFILE%") >= 0)
    {
        m_command.replace("%LOGFILE%", m_logFile);
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(": '%LOGFILE%' replaced with '%1' in cmd: '%2'")
            .arg(m_logFile, m_command));
    }

    if (!m_logging.isEmpty() && m_command.indexOf("%LOGGING%") >= 0)
    {
        m_command.replace("%LOGGING%", m_logging);
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(": '%LOGGING%' replaced with '%1' in cmd: '%2'")
            .arg(m_logging, m_command));
    }

    m_desc.replace("%URL%", url);

    if (!tunecmd.isEmpty())
    {
        QStringList args = MythCommandLineParser::MythSplitCommandString(tunecmd);
        QString cmd = args.takeFirst();
        m_tuningChannel = channum;
        m_tuneProc.start(cmd, args);
        if (!m_tuneProc.waitForStarted())
        {
            QString errmsg = QString("Tune `%1` failed: ").arg(tunecmd) + ENO;
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("TuneChannel", serial, errmsg, "ERR");
            return;
        }

        if (background)
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString(": Started in background `%1` URL '%2'")
                .arg(tunecmd, url));

            m_tunedChannel = m_tuningChannel;
            m_tuningChannel.clear();
            emit SetDescription(Desc());
            emit SendMessage("TuneChannel", serial,
                             QString("Tuned `%1`").arg(m_tunedChannel), "OK");
        }
        else
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC + QString(": Started `%1` URL '%2'")
                .arg(tunecmd, url));
            emit SendMessage("TuneChannel", serial,
                             QString("InProgress `%1`").arg(tunecmd), "OK");
        }
    }
    else
    {
        m_tunedChannel = channum;
        emit SetDescription(Desc());
        emit SendMessage("TuneChannel", serial,
                         QString("Tuned to %1").arg(m_tunedChannel), "OK");
    }
}

Q_SLOT void MythExternRecApp::TuneStatus(const QString & serial)
{
    if (m_tuneProc.state() == QProcess::Running)
    {
        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString(": Tune process(%1) still running").arg(m_tuneProc.processId()));
        emit SendMessage("TuneStatus", serial, "InProgress", "OK");
        return;
    }

    if (!m_tuneCommand.isEmpty() &&
        m_tuneProc.exitStatus() != QProcess::NormalExit)
    {
        QString errmsg = QString("'%1' failed: ")
                         .arg(m_tuneProc.program()) + ENO;
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
        emit SendMessage("TuneStatus", serial, errmsg, "WARN");
        return;
    }

    m_tunedChannel = m_tuningChannel;
    m_tuningChannel.clear();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString(": Tuned %1").arg(m_tunedChannel));
    emit SetDescription(Desc());
    emit SendMessage("TuneChannel", serial,
                     QString("Tuned to %1").arg(m_tunedChannel), "OK");
}

Q_SLOT void MythExternRecApp::LockTimeout(const QString & serial)
{
    if (!Open())
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "Cannot read LockTimeout from config file.");
        emit SendMessage("LockTimeout", serial, "Not open", "ERR");
        return;
    }

    if (m_lockTimeout > 0)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Using configured LockTimeout of %1").arg(m_lockTimeout));
        emit SendMessage("LockTimeout", serial, QString::number(m_lockTimeout), "OK");
        return;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        "No LockTimeout defined in config, defaulting to 12000ms");
    emit SendMessage("LockTimeout", serial,
                     m_scanCommand.isEmpty() ? "12000" : "120000", "OK");
}

Q_SLOT void MythExternRecApp::HasTuner(const QString & serial)
{
    emit SendMessage("HasTuner", serial, m_tuneCommand.isEmpty() &&
                     m_channelsIni.isEmpty() ? "No" : "Yes", "OK");
}

Q_SLOT void MythExternRecApp::HasPictureAttributes(const QString & serial)
{
    emit SendMessage("HasPictureAttributes", serial, "No", "OK");
}

Q_SLOT void MythExternRecApp::SetBlockSize(const QString & serial, int blksz)
{
    m_blockSize = blksz;
    emit SendMessage("BlockSize", serial, QString("Blocksize %1").arg(blksz), "OK");
}

Q_SLOT void MythExternRecApp::StartStreaming(const QString & serial)
{
    m_streaming = true;
    if (m_tunedChannel.isEmpty() && !m_channelsIni.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": No channel has been tuned");
        emit SendMessage("StartStreaming", serial,
                         "No channel has been tuned", "ERR");
        return;
    }

    if (m_proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Application already running");
        emit SendMessage("StartStreaming", serial,
                         "Application already running", "WARN");
        return;
    }

    QString streamcmd = m_command;

    QStringList args = MythCommandLineParser::MythSplitCommandString(streamcmd);
    QString cmd = args.takeFirst();
    m_proc.start(cmd, args, QIODevice::ReadOnly|QIODevice::Unbuffered);
    m_proc.setTextModeEnabled(false);
    m_proc.setReadChannel(QProcess::StandardOutput);

    LOG(VB_RECORD, LOG_INFO, LOC + QString(": Starting process '%1' args: '%2'")
        .arg(m_proc.program(), m_proc.arguments().join(' ')));

    if (!m_proc.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Failed to start application.");
        emit SendMessage("StartStreaming", serial,
                         "Failed to start application.", "ERR");
        return;
    }

    std::this_thread::sleep_for(50ms);

    if (m_proc.state() != QProcess::Running)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Application failed to start");
        emit SendMessage("StartStreaming", serial,
                         "Application failed to start", "ERR");
        return;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString(": Started process '%1' PID %2")
        .arg(m_proc.program()).arg(m_proc.processId()));

    emit Streaming(true);
    emit SetDescription(Desc());
    emit SendMessage("StartStreaming", serial, "Streaming Started", "OK");
}

Q_SLOT void MythExternRecApp::StopStreaming(const QString & serial, bool silent)
{
    m_streaming = false;
    if (m_proc.state() == QProcess::Running)
    {
        TerminateProcess(m_proc, "App");

        LOG(VB_RECORD, LOG_INFO, LOC + ": External application terminated.");
        if (silent)
            emit SendMessage("StopStreaming", serial, "Streaming Stopped", "STATUS");
        else
            emit SendMessage("StopStreaming", serial, "Streaming Stopped", "OK");
    }
    else
    {
        if (silent)
        {
            emit SendMessage("StopStreaming", serial,
                             "Already not Streaming", "INFO");
        }
        else
        {
            emit SendMessage("StopStreaming", serial,
                             "Already not Streaming", "WARN");
        }
    }

    emit Streaming(false);
    emit SetDescription(Desc());
}

Q_SLOT void MythExternRecApp::ProcStarted(void)
{
    QString msg = QString("Process '%1' started").arg(m_proc.program());
    LOG(VB_RECORD, LOG_INFO, LOC + ": " + msg);
    MythLog(msg);
}

Q_SLOT void MythExternRecApp::ProcFinished(int exitCode,
                                           QProcess::ExitStatus exitStatus)
{
    m_result = exitCode;
    QString msg = QString("%1Finished: %2 (exit code: %3)")
        .arg(exitStatus != QProcess::NormalExit ? "WARN:" : "",
             exitStatus == QProcess::NormalExit ? "OK" : "Abnormal exit",
             QString::number(m_result));
    LOG(VB_RECORD, LOG_INFO, LOC + ": " + msg);

    if (m_streaming)
        emit Streaming(false);
    MythLog(msg);
}

Q_SLOT void MythExternRecApp::ProcStateChanged(QProcess::ProcessState newState)
{
    bool unexpected = false;
    QString msg = "State Changed: ";
    switch (newState)
    {
        case QProcess::NotRunning:
          msg += "Not running";
          unexpected = m_streaming;
          break;
        case QProcess::Starting:
          msg += "Starting ";
          break;
        case QProcess::Running:
          msg += QString("Running PID %1").arg(m_proc.processId());
          break;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + msg);
    if (unexpected)
    {
        emit Streaming(false);
        emit SendMessage("STATUS", "0", "Unexpected: " + msg, "ERR");
    }
}

Q_SLOT void MythExternRecApp::ProcError(QProcess::ProcessError /*error */)
{
    if (m_terminating)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString(": %1")
            .arg(m_proc.errorString()));
        emit SendMessage("STATUS", "0", m_proc.errorString(), "INFO");
    }
    else
    {
        LOG(VB_RECORD, LOG_ERR, LOC + QString(": Error: %1")
            .arg(m_proc.errorString()));
        emit SendMessage("STATUS", "0", m_proc.errorString(), "ERR");
    }
}

Q_SLOT void MythExternRecApp::ProcReadStandardError(void)
{
    QByteArray buf = m_proc.readAllStandardError();
    QString    msg = QString::fromUtf8(buf).trimmed();
    QList<QString> msgs = msg.split('\n');
    QString    message;

    for (int idx=0; idx < msgs.count(); ++idx)
    {
        // Log any error messages
        if (!msgs[idx].isEmpty())
        {
            QStringList tokens = QString(msgs[idx])
                                 .split(':', Qt::SkipEmptyParts);
            tokens.removeFirst();
            if (tokens.empty())
                message = msgs[idx];
            else
                message = tokens.join(':');
            if (msgs[idx].startsWith("err", Qt::CaseInsensitive))
            {
                LOG(VB_RECORD, LOG_ERR, LOC + QString(">>> %1").arg(msgs[idx]));
                emit SendMessage("STATUS", "0", message, "ERR");
            }
            else if (msgs[idx].startsWith("warn", Qt::CaseInsensitive))
            {
                LOG(VB_RECORD, LOG_WARNING, LOC + QString(">>> %1").arg(msgs[idx]));
                emit SendMessage("STATUS", "0", message, "WARN");
            }
            else
            {
                LOG(VB_RECORD, LOG_DEBUG, LOC + QString(">>> %1").arg(msgs[idx]));
                emit SendMessage("STATUS", "0", message, "INFO");
            }
        }
    }
}

Q_SLOT void MythExternRecApp::ProcReadStandardOutput(void)
{
    LOG(VB_RECORD, LOG_WARNING, LOC + ": Data ready.");

    QByteArray buf = m_proc.read(m_blockSize);
    if (!buf.isEmpty())
        emit Fill(buf);
}

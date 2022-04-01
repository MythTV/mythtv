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

#include <thread>
#include <csignal>
#include "mythcommandlineparser.h"
#include "MythExternRecApp.h"
#include "mythchrono.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QtCore/QtCore>
#include <unistd.h>

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
}

MythExternRecApp::~MythExternRecApp(void)
{
    Close();
}

QString MythExternRecApp::Desc(void) const
{
    QString extra;

    if (m_proc.processId() > 0)
        extra = QString("(pid %1) ").arg(m_proc.processId());

    return QString("%1%2 ").arg(extra, m_desc);
}

bool MythExternRecApp::config(void)
{
    QSettings settings(m_configIni, QSettings::IniFormat);

    if (!settings.contains("RECORDER/command"))
    {
        LOG(VB_GENERAL, LOG_CRIT, "ini file missing [RECORDER]/command");
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
            QDir conf_path = QFileInfo(m_configIni).absolutePath();
            QFileInfo ini(conf_path, m_channelsIni);
            m_channelsIni = ini.absoluteFilePath();
        }
    }

    return true;
}

bool MythExternRecApp::Open(void)
{
    if (m_fatal)
    {
        emit SendMessage("Open", "0", "ERR:Already dead.");
        return false;
    }

    if (m_command.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": No recorder provided.");
        emit SendMessage("Open", "0", "ERR:No recorder provided.");
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
    if (proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGINT to %1(%2)").arg(desc).arg(proc.processId()));
        kill(proc.processId(), SIGINT);
        proc.waitForFinished(5000);
    }
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

    if (m_onDataStart.isEmpty())
        return;

    QString cmd = m_onDataStart;
    cmd.replace("%CHANNUM%", m_tunedChannel);

    bool background = false;
    int pos = cmd.lastIndexOf(QChar('&'));
    if (pos > 0)
    {
        background = true;
        cmd = cmd.left(pos);
    }

    QStringList args = MythCommandLineParser::MythSplitCommandString(cmd);
    cmd = args.takeFirst();

    TerminateProcess(m_finishTuneProc, "FinishTuning");

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Finishing tune: '%1' %3")
        .arg(m_onDataStart, background ? "in the background" : ""));

    m_finishTuneProc.start(cmd, args);
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
        emit SendMessage("LoadChannels", serial, "ERR:No channels configured.");
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
            emit SendMessage("LoadChannels", serial,
                             QString("ERR:%1").arg(errmsg));
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
            emit SendMessage("LoadChannels", serial,
                             QString("ERR:%1").arg(errmsg));
            return;
        }
    }

    if (m_chanSettings == nullptr)
        m_chanSettings = new QSettings(m_channelsIni, QSettings::IniFormat);
    m_chanSettings->sync();
    m_channels = m_chanSettings->childGroups();

    emit SendMessage("LoadChannels", serial,
                     QString("OK:%1").arg(m_channels.size()));
}

void MythExternRecApp::GetChannel(const QString & serial, const QString & func)
{
    if (m_channelsIni.isEmpty() || m_channels.empty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("FirstChannel", serial,
                         QString("ERR:No channels configured."));
        return;
    }

    if (m_chanSettings == nullptr)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": Invalid channel configuration.");
        emit SendMessage(func, serial,
                         "ERR:Invalid channel configuration.");
        return;
    }

    if (m_channels.size() <= m_channelIdx)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": No more channels.");
        emit SendMessage(func, serial, "ERR:No more channels.");
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

    emit SendMessage(func, serial, QString("OK:%1,%2,%3,%4,%5")
                     .arg(channum, name, callsign,
                          xmltvid, icon));
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

void MythExternRecApp::NewEpisodeStarting(const QString & channum)
{
    QString cmd = m_newEpisodeCommand;
    cmd.replace("%CHANNUM%", channum);

    QStringList args = MythCommandLineParser::MythSplitCommandString(cmd);
    cmd = args.takeFirst();

    LOG(VB_RECORD, LOG_WARNING, LOC +
        QString(" New episode starting on current channel: '%1'").arg(cmd));

    QProcess proc;
    proc.start(cmd, args);
    if (!proc.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC +
            " NewEpisodeStarting: Failed to start process: " + ENO);
        return;
    }
    proc.waitForFinished(5000);
    if (proc.state() == QProcess::NotRunning)
    {
        if (proc.exitStatus() != QProcess::NormalExit)
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                " NewEpisodeStarting: process failed: " + ENO);
            return;
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "NewEpisodeStarting: finished.");
}

Q_SLOT void MythExternRecApp::TuneChannel(const QString & serial,
                                          const QString & channum)
{
    if (m_tuneCommand.isEmpty() && m_channelsIni.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No 'tuner' configured.");
        emit SendMessage("TuneChannel", serial, "ERR:No 'tuner' configured.");
        return;
    }

    if (m_tunedChannel == channum)
    {
        if (!m_newEpisodeCommand.isEmpty())
            NewEpisodeStarting(channum);

        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("TuneChanne: Already on %1").arg(channum));
        emit SendMessage("TuneChannel", serial,
                         QString("OK:Tunned to %1").arg(channum));
        return;
    }

    m_desc    = m_recDesc;
    m_command = m_recCommand;

    QString tunecmd = m_tuneCommand;
    QString url;
    bool    background = false;

    int pos = tunecmd.lastIndexOf(QChar('&'));
    if (pos > 0)
    {
        background = true;
        tunecmd = tunecmd.left(pos);
    }

    if (!m_channelsIni.isEmpty())
    {
        QSettings settings(m_channelsIni, QSettings::IniFormat);
        settings.beginGroup(channum);

        url = settings.value("URL").toString();

        if (url.isEmpty())
        {
            QString msg = QString("Channel number [%1] is missing a URL.")
                          .arg(channum);

            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + msg);
        }
        else
            tunecmd.replace("%URL%", url);

        if (!url.isEmpty() && m_command.indexOf("%URL%") >= 0)
        {
            m_command.replace("%URL%", url);
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString(": '%URL%' replaced with '%1' in cmd: '%2'")
                .arg(url, m_command));
        }

        m_desc.replace("%CHANNAME%", settings.value("NAME").toString());
        m_desc.replace("%CALLSIGN%", settings.value("CALLSIGN").toString());

        settings.endGroup();
    }

    if (m_tuneProc.state() == QProcess::Running)
        TerminateProcess(m_tuneProc, "Tune");

    tunecmd.replace("%CHANNUM%", channum);
    m_command.replace("%CHANNUM%", channum);

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
    m_desc.replace("%CHANNUM%", channum);

    if (!m_tuneCommand.isEmpty())
    {
        QStringList args = MythCommandLineParser::MythSplitCommandString(tunecmd);
        QString cmd = args.takeFirst();
        m_tuningChannel = channum;
        m_tuneProc.start(cmd, args);
        if (!m_tuneProc.waitForStarted())
        {
            QString errmsg = QString("Tune `%1` failed: ").arg(tunecmd) + ENO;
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("TuneChannel", serial,
                             QString("ERR:%1").arg(errmsg));
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
                             QString("OK:Tuned `%1`").arg(m_tunedChannel));
        }
        else
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC + QString(": Started `%1` URL '%2'")
                .arg(tunecmd, url));
            emit SendMessage("TuneChannel", serial,
                             QString("OK:InProgress `%1`").arg(tunecmd));
        }
    }
    else
    {
        m_tunedChannel = channum;
        emit SetDescription(Desc());
        emit SendMessage("TuneChannel", serial,
                         QString("OK:Tuned to %1").arg(m_tunedChannel));
    }
}

Q_SLOT void MythExternRecApp::TuneStatus(const QString & serial)
{
    if (m_tuneProc.state() == QProcess::Running)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString(": Tune process(%1) still running").arg(m_tuneProc.processId()));
        emit SendMessage("TuneStatus", serial, "OK:InProgress");
        return;
    }

    if (!m_tuneCommand.isEmpty() &&
        m_tuneProc.exitStatus() != QProcess::NormalExit)
    {
        QString errmsg = QString("'%1' failed: ")
                         .arg(m_tuneProc.program()) + ENO;
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
        emit SendMessage("TuneStatus", serial,
                         QString("ERR:%1").arg(errmsg));
        return;
    }

    m_tunedChannel = m_tuningChannel;
    m_tuningChannel.clear();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString(": Tuned %1").arg(m_tunedChannel));
    emit SetDescription(Desc());
    emit SendMessage("TuneChannel", serial,
                     QString("OK:Tuned to %1").arg(m_tunedChannel));
}

Q_SLOT void MythExternRecApp::LockTimeout(const QString & serial)
{
    if (!Open())
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "Cannot read LockTimeout from config file.");
        emit SendMessage("LockTimeout", serial, "ERR: Not open");
        return;
    }

    if (m_lockTimeout > 0)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Using configured LockTimeout of %1").arg(m_lockTimeout));
        emit SendMessage("LockTimeout", serial,
                         QString("OK:%1").arg(m_lockTimeout));
        return;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        "No LockTimeout defined in config, defaulting to 12000ms");
    emit SendMessage("LockTimeout", serial, QString("OK:%1")
                     .arg(m_scanCommand.isEmpty() ? 12000 : 120000));
}

Q_SLOT void MythExternRecApp::HasTuner(const QString & serial)
{
    emit SendMessage("HasTuner", serial, QString("OK:%1")
                     .arg(m_tuneCommand.isEmpty() &&
                          m_channelsIni.isEmpty() ? "No" : "Yes"));
}

Q_SLOT void MythExternRecApp::HasPictureAttributes(const QString & serial)
{
    emit SendMessage("HasPictureAttributes", serial, "OK:No");
}

Q_SLOT void MythExternRecApp::SetBlockSize(const QString & serial, int blksz)
{
    m_blockSize = blksz;
    emit SendMessage("BlockSize", serial, "OK");
}

Q_SLOT void MythExternRecApp::StartStreaming(const QString & serial)
{
    m_streaming = true;
    if (m_tunedChannel.isEmpty() && !m_channelsIni.isEmpty())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": No channel has been tuned");
        emit SendMessage("StartStreaming", serial,
                         "ERR:No channel has been tuned");
        return;
    }

    if (m_proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Application already running");
        emit SendMessage("StartStreaming", serial,
                         "WARN:Application already running");
        return;
    }

    QStringList args = MythCommandLineParser::MythSplitCommandString(m_command);
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
                         "ERR:Failed to start application.");
        return;
    }

    std::this_thread::sleep_for(50ms);

    if (m_proc.state() != QProcess::Running)
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Application failed to start");
        emit SendMessage("StartStreaming", serial,
                         "ERR:Application failed to start");
        return;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString(": Started process '%1' PID %2")
        .arg(m_proc.program()).arg(m_proc.processId()));

    emit Streaming(true);
    emit SetDescription(Desc());
    emit SendMessage("StartStreaming", serial, "OK:Streaming Started");
}

Q_SLOT void MythExternRecApp::StopStreaming(const QString & serial, bool silent)
{
    m_streaming = false;
    if (m_proc.state() == QProcess::Running)
    {
        TerminateProcess(m_proc, "App");

        LOG(VB_RECORD, LOG_INFO, LOC + ": External application terminated.");
        if (silent)
            emit SendMessage("StopStreaming", serial, "STATUS:Streaming Stopped");
        else
            emit SendMessage("StopStreaming", serial, "OK:Streaming Stopped");
    }
    else
    {
        if (silent)
        {
            emit SendMessage("StopStreaming", serial,
                             "STATUS:Already not Streaming");
        }
        else
        {
            emit SendMessage("StopStreaming", serial,
                             "WARN:Already not Streaming");
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
        MythLog("ERR Unexpected " + msg);
    }
}

Q_SLOT void MythExternRecApp::ProcError(QProcess::ProcessError /*error */)
{
    LOG(VB_RECORD, LOG_ERR, LOC + QString(": Error: %1")
        .arg(m_proc.errorString()));
    MythLog(m_proc.errorString());
}

Q_SLOT void MythExternRecApp::ProcReadStandardError(void)
{
    QByteArray buf = m_proc.readAllStandardError();
    QString    msg = QString::fromUtf8(buf).trimmed();

    // Log any error messages
    if (!msg.isEmpty())
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString(">>> %1")
            .arg(msg));
        if (msg.size() > 79)
            msg = QString("Application message: see '%1'").arg(m_logFile);

        MythLog(msg);
    }
}

Q_SLOT void MythExternRecApp::ProcReadStandardOutput(void)
{
    LOG(VB_RECORD, LOG_WARNING, LOC + ": Data ready.");

    QByteArray buf = m_proc.read(m_blockSize);
    if (!buf.isEmpty())
        emit Fill(buf);
}

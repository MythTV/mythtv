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
#include "commandlineparser.h"
#include "MythExternRecApp.h"

#include <QtCore/QtCore>
#include <QFileInfo>
#include <QProcess>
#include <QElapsedTimer>

#define LOC Desc()

MythExternRecApp::MythExternRecApp(const QString & command,
                                   const QString & conf_file,
                                   const QString & log_file,
                                   const QString & logging)
    : m_fatal(false)
    , m_run(true)
    , m_streaming(false)
    , m_result(0)
    , m_buffer_max(188 * 10000)
    , m_block_size(m_buffer_max / 4)
    , m_rec_command(command)
    , m_lock_timeout(0)
    , m_scan_timeout(120000)
    , m_log_file(log_file)
    , m_logging(logging)
    , m_config_ini(conf_file)
    , m_tuned(false)
    , m_chan_settings(nullptr)
    , m_channel_idx(-1)
{
    if (m_config_ini.isEmpty() || !config())
        m_rec_desc = m_rec_command;

    if (m_tune_command.isEmpty())
        m_command = m_rec_command;

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Channels in '%1', Tuner: '%2', Scanner: '%3'")
        .arg(m_channels_ini).arg(m_tune_command).arg(m_scan_command));

    m_desc = m_rec_desc;
    m_desc.replace("%URL%", "");
    m_desc.replace("%CHANNUM%", "");
    m_desc.replace("%CHANNAME%", "");
    m_desc.replace("%CALLSIGN%", "");
    emit SetDescription(m_desc);
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

    return QString("%1%2 ").arg(extra).arg(m_desc);
}

bool MythExternRecApp::config(void)
{
    QSettings settings(m_config_ini, QSettings::IniFormat);

    if (!settings.contains("RECORDER/command"))
    {
        LOG(VB_GENERAL, LOG_CRIT, "ini file missing [RECORDER]/command");
        m_fatal = true;
        return false;
    }

    m_rec_command  = settings.value("RECORDER/command").toString();
    m_rec_desc     = settings.value("RECORDER/desc").toString();
    m_tune_command = settings.value("TUNER/command", "").toString();
    m_channels_ini = settings.value("TUNER/channels", "").toString();
    m_lock_timeout = settings.value("TUNER/timeout", "").toInt();
    m_scan_command = settings.value("SCANNER/command", "").toString();
    m_scan_timeout = settings.value("SCANNER/timeout", "").toInt();

    settings.beginGroup("ENVIRONMENT");

    m_app_env.clear();
    QStringList keys = settings.childKeys();
    QStringList::const_iterator Ienv;
    for (Ienv = keys.constBegin(); Ienv != keys.constEnd(); ++Ienv)
    {
        if (!(*Ienv).isEmpty() && (*Ienv)[0] != '#')
            m_app_env.insert((*Ienv).toLocal8Bit().constData(),
                             settings.value(*Ienv).toString());
    }

    if (!m_channels_ini.isEmpty())
    {
        if (!QFileInfo::exists(m_channels_ini))
        {
            // Assume the channels config is in the same directory as
            // main config
            QDir conf_path = QFileInfo(m_config_ini).absolutePath();
            QFileInfo ini(conf_path, m_channels_ini);
            m_channels_ini = ini.absoluteFilePath();
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

    if (!m_app_env.isEmpty())
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QMap<QString, QString>::const_iterator Ienv;
        for (Ienv = m_app_env.constBegin();
             Ienv != m_app_env.constEnd(); ++Ienv)
        {
            env.insert(Ienv.key(), Ienv.value());
            LOG(VB_RECORD, LOG_INFO, LOC + QString(" ENV: '%1' = '%2'")
                .arg(Ienv.key()).arg(Ienv.value()));
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    QObject::connect(&m_proc, &QProcess::errorOccurred,
                     this, &MythExternRecApp::ProcError);
#endif

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

void MythExternRecApp::TerminateProcess(void)
{
    if (m_proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGINT to %1").arg(m_proc.pid()));
        kill(m_proc.pid(), SIGINT);
        m_proc.waitForFinished(5000);
    }
    if (m_proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGTERM to %1").arg(m_proc.pid()));
        m_proc.terminate();
        m_proc.waitForFinished();
    }
    if (m_proc.state() == QProcess::Running)
    {
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Sending SIGKILL to %1").arg(m_proc.pid()));
        m_proc.kill();
        m_proc.waitForFinished();
    }
}

Q_SLOT void MythExternRecApp::Close(void)
{
    if (m_run)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + ": Closing application.");
        m_run = false;
        m_run_cond.notify_all();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    if (m_proc.state() == QProcess::Running)
    {
        m_proc.closeReadChannel(QProcess::StandardOutput);
        TerminateProcess();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    emit Done();
}

void MythExternRecApp::Run(void)
{
    QByteArray buf;

    while (m_run)
    {
        {
            std::unique_lock<std::mutex> lk(m_run_mutex);
            m_run_cond.wait_for(lk, std::chrono::milliseconds(10));
        }

        if (m_proc.state() == QProcess::Running)
        {
            if (m_proc.waitForReadyRead(50))
            {
                buf = m_proc.read(m_block_size);
                if (!buf.isEmpty())
                    emit Fill(buf);
            }
        }

        qApp->processEvents();
    }

    if (m_proc.state() == QProcess::Running)
    {
        m_proc.closeReadChannel(QProcess::StandardOutput);
        TerminateProcess();
    }

    emit Done();
}

Q_SLOT void MythExternRecApp::LoadChannels(const QString & serial)
{
    if (m_channels_ini.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("LoadChannels", serial, "ERR:No channels configured.");
        return;
    }

    if (!m_scan_command.isEmpty())
    {
        QString cmd = m_scan_command;
        cmd.replace("%CHANCONF%", m_channels_ini);

        QProcess scanner;
        scanner.start(cmd);

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
        while (timer.elapsed() < m_scan_timeout)
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
        if (timer.elapsed() >= m_scan_timeout)
        {
            QString errmsg = QString("Timedout waiting for '%1'").arg(cmd);
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("LoadChannels", serial,
                             QString("ERR:%1").arg(errmsg));
            return;
        }
    }

    if (m_chan_settings == nullptr)
        m_chan_settings = new QSettings(m_channels_ini,
                                        QSettings::IniFormat);
    m_chan_settings->sync();
    m_channels = m_chan_settings->childGroups();

    emit SendMessage("LoadChannels", serial,
                     QString("OK:%1").arg(m_channels.size()));
}

void MythExternRecApp::GetChannel(const QString & serial, const QString & func)
{
    if (m_channels_ini.isEmpty() || m_channels.empty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("FirstChannel", serial,
                         QString("ERR:No channels configured."));
        return;
    }

    if (m_chan_settings == nullptr)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": Invalid channel configuration.");
        emit SendMessage(func, serial,
                         "ERR:Invalid channel configuration.");
        return;
    }

    if (m_channels.size() <= m_channel_idx)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + ": No more channels.");
        emit SendMessage(func, serial, "ERR:No more channels.");
        return;
    }

    QString channum = m_channels[m_channel_idx++];

    m_chan_settings->beginGroup(channum);

    QString name     = m_chan_settings->value("NAME").toString();
    QString callsign = m_chan_settings->value("CALLSIGN").toString();
    QString xmltvid  = m_chan_settings->value("XMLTVID").toString();

    m_chan_settings->endGroup();

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString(": NextChannel Name:'%1',Callsign:'%2',xmltvid:%3")
        .arg(name).arg(callsign).arg(xmltvid));

    emit SendMessage(func, serial, QString("OK:%1,%2,%3,%4")
                     .arg(channum).arg(name).arg(callsign).arg(xmltvid));
}

Q_SLOT void MythExternRecApp::FirstChannel(const QString & serial)
{
    m_channel_idx = 0;
    GetChannel(serial, "FirstChannel");
}

Q_SLOT void MythExternRecApp::NextChannel(const QString & serial)
{
    GetChannel(serial, "NextChannel");
}

Q_SLOT void MythExternRecApp::TuneChannel(const QString & serial,
                                          const QString & channum)
{
    if (m_channels_ini.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_ERR, LOC + ": No channels configured.");
        emit SendMessage("TuneChannel", serial, "ERR:No channels configured.");
        return;
    }

    QSettings settings(m_channels_ini, QSettings::IniFormat);
    settings.beginGroup(channum);

    QString url(settings.value("URL").toString());

    if (url.isEmpty())
    {
        QString msg = QString("Channel number [%1] is missing a URL.")
                      .arg(channum);

        LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + msg);

        emit SendMessage("TuneChannel", serial, QString("ERR:%1").arg(msg));
        return;
    }

    if (!m_tune_command.isEmpty())
    {
        // Repalce URL in command and execute it
        QString tune = m_tune_command;
        tune.replace("%URL%", url);

        if (system(tune.toUtf8().constData()) != 0)
        {
            QString errmsg = QString("'%1' failed: ").arg(tune) + ENO;
            LOG(VB_CHANNEL, LOG_ERR, LOC + ": " + errmsg);
            emit SendMessage("TuneChannel", serial, QString("ERR:%1").arg(errmsg));
            return;
        }
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString(": TuneChannel, ran '%1'").arg(tune));
    }

    // Replace URL in recorder command
    m_command = m_rec_command;

    if (!url.isEmpty() && m_command.indexOf("%URL%") >= 0)
    {
        m_command.replace("%URL%", url);
        LOG(VB_CHANNEL, LOG_DEBUG, LOC +
            QString(": '%URL%' replaced with '%1' in cmd: '%2'")
            .arg(url).arg(m_command));
    }

    if (!m_log_file.isEmpty() && m_command.indexOf("%LOGFILE%") >= 0)
    {
        m_command.replace("%LOGFILE%", m_log_file);
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(": '%LOGFILE%' replaced with '%1' in cmd: '%2'")
            .arg(m_log_file).arg(m_command));
    }

    if (!m_logging.isEmpty() && m_command.indexOf("%LOGGING%") >= 0)
    {
        m_command.replace("%LOGGING%", m_logging);
        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString(": '%LOGGING%' replaced with '%1' in cmd: '%2'")
            .arg(m_logging).arg(m_command));
    }

    m_desc = m_rec_desc;
    m_desc.replace("%URL%", url);
    m_desc.replace("%CHANNUM%", channum);
    m_desc.replace("%CHANNAME%", settings.value("NAME").toString());
    m_desc.replace("%CALLSIGN%", settings.value("CALLSIGN").toString());

    settings.endGroup();

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString(": TuneChannel %1: URL '%2'").arg(channum).arg(url));
    m_tuned = true;

    emit SetDescription(Desc());
    emit SendMessage("TuneChannel", serial,
                     QString("OK:Tunned to %1").arg(channum));
}

Q_SLOT void MythExternRecApp::LockTimeout(const QString & serial)
{
    if (!Open())
        return;

    if (m_lock_timeout > 0)
        emit SendMessage("LockTimeout", serial,
                         QString("OK:%1").arg(m_lock_timeout));
    emit SendMessage("LockTimeout", serial, QString("OK:%1")
                     .arg(m_scan_command.isEmpty() ? 12000 : 120000));
}

Q_SLOT void MythExternRecApp::HasTuner(const QString & serial)
{
    emit SendMessage("HasTuner", serial, QString("OK:%1")
                     .arg(m_channels_ini.isEmpty() ? "No" : "Yes"));
}

Q_SLOT void MythExternRecApp::HasPictureAttributes(const QString & serial)
{
    emit SendMessage("HasPictureAttributes", serial, "OK:No");
}

Q_SLOT void MythExternRecApp::SetBlockSize(const QString & serial, int blksz)
{
    m_block_size = blksz;
    emit SendMessage("BlockSize", serial, "OK");
}

Q_SLOT void MythExternRecApp::StartStreaming(const QString & serial)
{
    m_streaming = true;
    if (!m_tuned && !m_channels_ini.isEmpty())
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

    m_proc.start(m_command, QIODevice::ReadOnly|QIODevice::Unbuffered);
    m_proc.setTextModeEnabled(false);
    m_proc.setReadChannel(QProcess::StandardOutput);

    LOG(VB_RECORD, LOG_INFO, LOC + QString(": Starting process '%1' args: '%2'")
        .arg(m_proc.program()).arg(m_proc.arguments().join(' ')));

    if (!m_proc.waitForStarted())
    {
        LOG(VB_RECORD, LOG_ERR, LOC + ": Failed to start application.");
        emit SendMessage("StartStreaming", serial,
                         "ERR:Failed to start application.");
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

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
        TerminateProcess();

        LOG(VB_RECORD, LOG_INFO, LOC + ": External application terminated.");
        if (silent)
            emit SendMessage("StopStreaming", serial, "STATUS:Streaming Stopped");
        else
            emit SendMessage("StopStreaming", serial, "OK:Streaming Stopped");
    }
    else
    {
        if (silent)
            emit SendMessage("StopStreaming", serial,
                             "STATUS:Already not Streaming");
        else
            emit SendMessage("StopStreaming", serial,
                             "WARN:Already not Streaming");
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
                  .arg(exitStatus != QProcess::NormalExit ? "WARN:" : "")
                  .arg(exitStatus == QProcess::NormalExit ? "OK" :
                       "Abnormal exit")
                  .arg(m_result);
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
            msg = QString("Application message: see '%1'").arg(m_log_file);

        MythLog(msg);
    }
}

Q_SLOT void MythExternRecApp::ProcReadStandardOutput(void)
{
    LOG(VB_RECORD, LOG_WARNING, LOC + ": Data ready.");

    QByteArray buf = m_proc.read(m_block_size);
    if (!buf.isEmpty())
        emit Fill(buf);
}

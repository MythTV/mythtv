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

#ifndef MYTHTVEXTERNRECAPP_H
#define MYTHTVEXTERNRECAPP_H

#include <QDir>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QString>
#include <QVariantMap>

#include <atomic>
#include <condition_variable>

class MythExternControl;

class MythExternRecApp : public QObject
{
    Q_OBJECT

  public:
    MythExternRecApp(QString command,  QString conf_file,
                     QString log_file, QString logging);
    ~MythExternRecApp(void) override;

    bool Open(void);
    void Run(void);

    void ReplaceVariables(QString & cmd) const;
    QString Desc(void) const;
    void MythLog(const QString & msg)
    { emit SendMessage("", "0", "STATUS", msg); }
    void SetErrorMsg(const QString & msg) { emit ErrorMessage(msg); }

  signals:
    void SetDescription(const QString & desc);
    void SendMessage(const QString & command,
                     const QString & serial,
                     const QString & message,
                     const QString & status = "");
    void ErrorMessage(const QString & msg);
    void Opened(void);
    void Done(void);
    void Streaming(bool val);
    void Fill(const QByteArray & buffer);

  public slots:
    void ProcStarted(void);
    void ProcFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void ProcStateChanged(QProcess::ProcessState newState);
    void ProcError(QProcess::ProcessError error);
    void ProcReadStandardError(void);
    void ProcReadStandardOutput(void);

    void Close(void);
    void StartStreaming(const QString & serial);
    void StopStreaming(const QString & serial, bool silent);
    void LockTimeout(const QString & serial);
    void HasTuner(const QString & serial);
    void Cleanup(void);
    void DataStarted(void);
    void LoadChannels(const QString & serial);
    void FirstChannel(const QString & serial);
    void NextChannel(const QString & serial);

    void NewEpisodeStarting(void);
    void TuneChannel(const QString & serial, const QVariantMap & chaninfo);
    void TuneStatus(const QString & serial);
    void HasPictureAttributes(const QString & serial);
    void SetBlockSize(const QString & serial, int blksz);

  protected:
    void GetChannel(const QString & serial, const QString & func);
    void TerminateProcess(QProcess & proc, const QString & desc) const;

  private:
    bool config(void);
    static QString sanitize_var(const QString & var);
    QString replace_extra_args(const QString & var,
                               const QVariantMap & extra_args) const;

    mutable bool            m_terminating  { false };
    bool                    m_fatal        { false };
    QString                 m_fatalMsg;

    std::atomic<bool>       m_run          { true };
    std::condition_variable m_runCond;
    std::mutex              m_runMutex;
    std::atomic<bool>       m_streaming    { false };
    int                     m_result       { 0 };

    uint                    m_bufferMax    { 188 * 10000 };
    uint                    m_blockSize    { m_bufferMax / 4 };

    QProcess                m_proc;
    QString                 m_command;
    QString                 m_cleanup;

    QString                 m_recCommand;
    QString                 m_recDesc;

    QMap<QString, QString>  m_appEnv;
    QMap<QString, QString>  m_settingVars;
    QVariantMap             m_chaninfo;

    QProcess                m_tuneProc;
    QProcess                m_finishTuneProc;
    QString                 m_tuneCommand;
    QString                 m_onDataStart;
    QString                 m_newEpisodeCommand;
    QString                 m_channelsIni;
    uint                    m_lockTimeout  { 0 };

    QString                 m_scanCommand;
    uint                    m_scanTimeout  { 120000 };

    QString                 m_logFile;
    QString                 m_logging;
    QString                 m_configIni;
    QString                 m_desc;

    QString                 m_tuningChannel;
    QString                 m_tunedChannel;

    // Channel scanning
    QSettings              *m_chanSettings { nullptr };
    QStringList             m_channels;
    int                     m_channelIdx   { -1 };

};

#endif // MYTHTVEXTERNRECAPP_H

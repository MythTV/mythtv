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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Qt
#include <QCoreApplication>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"

// MythExternRecorder
#include "MythExternControl.h"
#include "MythExternRecApp.h"
#include "mythexternrecorder_commandlineparser.h"

int main(int argc, char *argv[])
{
    MythExternRecorderCommandLineParser cmdline;

    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        MythExternRecorderCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("mythexternrecorder");

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;
    QString logfile = cmdline.GetLogFilePath();
    QString logging = logPropagateArgs;

    auto *control = new MythExternControl();
    MythExternRecApp  *process = nullptr;

    QString conf_file = cmdline.toString("conf");
    if (!conf_file.isEmpty())
    {
        process = new MythExternRecApp("", conf_file, logfile, logging);
    }
    else if (!cmdline.toString("exec").isEmpty())
    {
        QString command = cmdline.toString("exec");
        process = new MythExternRecApp(command, "", logfile, logging);
    }
    else if (!cmdline.toString("infile").isEmpty())
    {
        QString filename = cmdline.toString("infile");
        QString command = QString("ffmpeg -re -i \"%1\" "
                                  "-c:v copy -c:a copy -f mpegts -")
                          .arg(filename);
        process = new MythExternRecApp(command, "", logfile, logging);
    }
    if (process == nullptr)
    {
        delete control;
        return GENERIC_EXIT_NOT_OK;
    }

    QObject::connect(process, &MythExternRecApp::Opened,
                     control, &MythExternControl::Opened);
    QObject::connect(process, &MythExternRecApp::Done,
                     control, &MythExternControl::Done);
    QObject::connect(process, &MythExternRecApp::SetDescription,
                     control, &MythExternControl::SetDescription);
    QObject::connect(process, &MythExternRecApp::SendMessage,
                     control, &MythExternControl::SendMessage);
    QObject::connect(process, &MythExternRecApp::ErrorMessage,
                     control, &MythExternControl::ErrorMessage);
    QObject::connect(process, &MythExternRecApp::Streaming,
                     control, &MythExternControl::Streaming);
    QObject::connect(process, &MythExternRecApp::Fill,
                     control, &MythExternControl::Fill);

    QObject::connect(control, &MythExternControl::Close,
                     process, &MythExternRecApp::Close);
    QObject::connect(control, &MythExternControl::StartStreaming,
                     process, &MythExternRecApp::StartStreaming);
    QObject::connect(control, &MythExternControl::StopStreaming,
                     process, &MythExternRecApp::StopStreaming);
    QObject::connect(control, &MythExternControl::LockTimeout,
                     process, &MythExternRecApp::LockTimeout);
    QObject::connect(control, &MythExternControl::HasTuner,
                     process, &MythExternRecApp::HasTuner);
    QObject::connect(control, &MythExternControl::Cleanup,
                     process, &MythExternRecApp::Cleanup);
    QObject::connect(control, &MythExternControl::DataStarted,
                     process, &MythExternRecApp::DataStarted);
    QObject::connect(control, &MythExternControl::LoadChannels,
                     process, &MythExternRecApp::LoadChannels);
    QObject::connect(control, &MythExternControl::FirstChannel,
                     process, &MythExternRecApp::FirstChannel);
    QObject::connect(control, &MythExternControl::NextChannel,
                     process, &MythExternRecApp::NextChannel);
    QObject::connect(control, &MythExternControl::TuneChannel,
                     process, &MythExternRecApp::TuneChannel);
    QObject::connect(control, &MythExternControl::TuneStatus,
                     process, &MythExternRecApp::TuneStatus);
    QObject::connect(control, &MythExternControl::HasPictureAttributes,
                     process, &MythExternRecApp::HasPictureAttributes);
    QObject::connect(control, &MythExternControl::SetBlockSize,
                     process, &MythExternRecApp::SetBlockSize);

    process->Run();

    delete process;
    delete control;
    logStop();

    LOG(VB_GENERAL, LOG_WARNING, "Finished.");

    return GENERIC_EXIT_OK;
}

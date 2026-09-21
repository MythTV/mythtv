/*  -*- Mode: c++ -*-
 *
 * Copyright (C) John Poet 2026
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
#include <cstdio>
#include <iostream>
#include <sys/prctl.h>

// Qt
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSocketNotifier>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "mythexternrecorder_commandlineparser.h"

// MythExternRecorder
#include "recorder.h"
#include "touchmonitor.h"
#include "stdinnotifier.h"

#if HAVE_TOMLPLUSPLUS
#include "config_toml.h"
#endif
#include "config_ini.h"

int main(int argc, char *argv[])
{
    MythExternRecorderCommandLineParser cmdline;

    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        std::cerr << "\nDocumentation can be found at\n"
                  << "https://github.com/MythTV/mythtv/blob/master/mythtv/programs/mythexternrecorder/README.md\n";

        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        std::cerr << "\nDocumentation can be found at\n"
                  << "https://github.com/MythTV/mythtv/blob/master/mythtv/programs/mythexternrecorder/README.md\n";

        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        MythExternRecorderCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHEXTERNRECORDER);
    QThread::currentThread()->setObjectName("mythextrec");

#if defined(Q_OS_LINUX)
    prctl(PR_SET_NAME, "mythextrec", 0, 0, 0);
#endif

    std::filesystem::path confFile = cmdline.toString("conf").toStdString();
    QString logPath = cmdline.toString("logpath");

    std::unique_ptr<ExternConfig> config;
    if (confFile.extension().string() == ".toml")
    {
#if HAVE_TOMLPLUSPLUS
        config = std::make_unique<ExternTomlConfig>(confFile);
#else
        std::cerr << "Myth was not built with toml support.\n";
        return 1;
#endif
    }
    else if (confFile.extension().string() == ".conf")
    {
        config = std::make_unique<ExternIniConfig>(confFile);
    }
    else
    {
        std::cerr << "Configuration file must end in .toml or .conf (for INI)\n";
        return 1;
    }

    if (config->failed())
    {
        std::cerr << "Invalid config file.\n";
        return 1;
    }

    // Init logging
    std::string desc = config->getValue("RECORDER", "desc",
                                         confFile.stem().string());
    QString logFile = logPath + QString("/recorder-%1.log")
                       .arg(QString::fromStdString(desc));

    cmdline.SetupLogging(logFile);

    LOG(VB_RECORD, LOG_CRIT,
        QString("Starting external recorder %1")
        .arg(QString::fromStdString(desc)));

    QTextStream stderrStream(stderr);

    StdinNotifier stdinNotifier;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QObject::connect(&stdinNotifier,
                     &StdinNotifier::apiVersionRequested,
                     &stdinNotifier,
                     [&stderrStream]()
                     {
                         stderrStream << "OK:3\n";
                         stderrStream.flush();
                     });

#else
    QObject::connect(&stdinNotifier,
                     &StdinNotifier::apiVersionRequested,
                     [&stderrStream]()
                     {
                         stderrStream << "OK:3\n";
                         stderrStream.flush();
                     });
#endif

    Recorder recorder(&app,
                      std::move(config),
                      QString::fromStdString(desc));

    QObject::connect(&stdinNotifier,
                     &StdinNotifier::commandReceived,
                     &recorder,
                     [&recorder, &app](const QJsonObject &json)
                     {
                         try
                         {
                             if (!recorder.processCommand(json))
                             {
                                 app.quit();
                             }
                         }
                         catch (const std::exception& e)
                         {
                             LOG(VB_RECORD, LOG_ERR,
                                 QString("Exception processing command: %1")
                                 .arg(e.what()));
                             app.quit();
                         }
                         catch (...)
                         {
                             LOG(VB_RECORD, LOG_ERR,
                                 "Unknown exception processing command.");
                             app.quit();
                         }
                     });

    QObject::connect(&stdinNotifier,
                     &StdinNotifier::finished,
                     &app,
                     &QCoreApplication::quit);

    // Start the Qt event loop
    int exitCode = app.exec();

    LOG(VB_RECORD, LOG_DEBUG, "Finished.");
    logStop();

    return exitCode;
}

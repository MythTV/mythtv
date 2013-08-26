// Copyright 2011-2012 Johann Duscher. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice, this list of
//       conditions and the following disclaimer.
//
//    2. Redistributions in binary form must reproduce the above copyright notice, this list
//       of conditions and the following disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY JOHANN DUSCHER ''AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// The views and conclusions contained in the software and documentation are those of the
// authors and should not be interpreted as representing official policies, either expressed
// or implied, of Johann Duscher.

#ifndef NZMQTAPP_H
#define NZMQTAPP_H

#include <stdexcept>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include "nzmqt/nzmqt.hpp"

#include "pubsub/PubSubClient.h"
#include "pubsub/PubSubServer.h"
#include "reqrep/ReqRepClient.h"
#include "reqrep/ReqRepServer.h"
#include "pushpull/PushPullVentilator.h"
#include "pushpull/PushPullWorker.h"
#include "pushpull/PushPullSink.h"


namespace nzmqt
{

namespace samples
{

class NzmqtApp : public QCoreApplication
{
    Q_OBJECT

    typedef QCoreApplication super;

public:
    explicit NzmqtApp(int& argc, char** argv)
        : super(argc, argv)
    {
        QTimer::singleShot(0, this, SLOT(run()));
    }

    bool notify(QObject *obj, QEvent *event)
    {
        try
        {
            return super::notify(obj, event);
        }
        catch (std::exception& ex)
        {
            qWarning() << ex.what();
            return false;
        }
    }

protected slots:
    void run()
    {
        QTextStream cout(stdout);
        try
        {
            QStringList args = arguments();

            if (args.size() == 1 || "-h" == args[1] || "--help" == args[1])
            {
                printUsage(cout);
                quit();
                return;
            }

            QString command = args[1];
            QRunnable* commandImpl = 0;

            if ("pubsub-server" == command)
            {
                if (args.size() < 4)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString address = args[2];
                QString topic = args[3];
                commandImpl = new PubSubServer(address, topic, this);
            }
            else if ("pubsub-client" == command)
            {
                if (args.size() < 4)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString address = args[2];
                QString topic = args[3];
                commandImpl = new PubSubClient(address, topic, this);
            }
            else if ("reqrep-server" == command)
            {
                if (args.size() < 4)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString address = args[2];
                QString responseMsg = args[3];
                commandImpl = new ReqRepServer(address, responseMsg, this);
            }
            else if ("reqrep-client" == command)
            {
                if (args.size() < 4)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString address = args[2];
                QString requestMsg = args[3];
                commandImpl = new ReqRepClient(address, requestMsg, this);
            }
            else if ("pushpull-ventilator" == command)
            {
                if (args.size() < 5)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString ventilatorAddress = args[2];
                QString sinkAddress = args[3];
                quint32 numberOfWorkItems = args[4].toUInt();
                commandImpl = new PushPullVentilator(ventilatorAddress, sinkAddress, numberOfWorkItems, this);
            }
            else if ("pushpull-worker" == command)
            {
                if (args.size() < 4)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString ventilatorAddress = args[2];
                QString sinkAddress = args[3];
                commandImpl = new PushPullWorker(ventilatorAddress, sinkAddress, this);
            }
            else if ("pushpull-sink" == command)
            {
                if (args.size() < 3)
                    throw std::runtime_error("Mandatory argument(s) missing!");

                QString sinkAddress = args[2];
                commandImpl = new PushPullSink(sinkAddress, this);
            }
            else
            {
                throw std::runtime_error(QString("Unknown command: '%1'").arg(command).toStdString());
            }

            // Run command.
            commandImpl->run();
        }
        catch (std::exception& ex)
        {
            qWarning() << ex.what();
            exit(-1);
        }
    }

protected:
    void printUsage(QTextStream& out)
    {
        QString executable = arguments().at(0);
        out << QString(
"\n\
USAGE: %1 [-h|--help]                                                                   -- Show this help message.\n\
\n\
USAGE: %1 <pubsub-server> <address> <topic>                                             -- Start PUB server.\n\
       %1 <pubsub-client> <address> <topic>                                             -- Start SUB client.\n\
\n\
USAGE: %1 <reqrep-server> <address> <reply-msg>                                         -- Start REQ server.\n\
       %1 <reqrep-client> <address> <request-msg>                                       -- Start REP client.\n\
\n\
USAGE: %1 <pushpull-ventilator> <ventilator-address> <sink-address> <numberOfWorkItems> -- Start ventilator.\n\
       %1 <pushpull-worker> <ventilator-address> <sink-address>                         -- Start a worker.\n\
       %1 <pushpull-sink> <sink-address>                                                -- Start sink.\n\
\n\
Publish-Subscribe Sample:\n\
* Server: %1 pubsub-server tcp://127.0.0.1:1234 ping\n\
* Client: %1 pubsub-client tcp://127.0.0.1:1234 ping\n\
\n\
Request-Reply Sample:\n\
* Server: %1 reqrep-server tcp://127.0.0.1:1234 World\n\
* Client: %1 reqrep-client tcp://127.0.0.1:1234 Hello\n\
\n\
Push-Pull Sample:\n\
* Ventilator:  %1 pushpull-ventilator tcp://127.0.0.1:5557 tcp://127.0.0.1:5558 100\n\
* Worker 1..n: %1 pushpull-worker tcp://127.0.0.1:5557 tcp://127.0.0.1:5558\n\
* Sink:        %1 pushpull-sink tcp://127.0.0.1:5558\n\
\n").arg(executable);
    }
};

}

}

#endif // NZMQTAPP_H

// ANSI C headers
#include <cstdlib>

// POSIX headers
#include <sys/types.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QHostAddress>
#include <QUdpSocket>
#include <QString>
#include <QFile>

// MythTV headers
#include "exitcodes.h"
#include "compat.h"
#include "mythcommandlineparser.h"

#ifndef VERBOSE
#define VB_IMPORTANT 0
#define VERBOSE(LEVEL, MSG) \
    do { cout << QString(MSG).toLocal8Bit().constData() << endl; } while (0)
#endif

const QString kMessage =
"<mythmessage version=\"1\">\n"
"  <text>%message_text%</text>\n"
"</mythmessage>";

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName("mythmessage");

    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    QString message = kMessage;

    bool verbose = false;

    MythMessageCommandLineParser cmdline;
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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("printtemplate"))
    {
        cerr << kMessage.toLocal8Bit().constData() << endl;
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("verbose"))
        verbose = true;
    if (cmdline.toBool("port"))
        port = (unsigned short)cmdline.toUInt("port");
    if (cmdline.toBool("addr"))
        address.setAddress(cmdline.toString("addr"));

    QMap<QString,QString>::const_iterator i;
    QMap<QString,QString> extras = cmdline.toMap("extra");
    for (i = extras.begin(); i != extras.end(); ++i)
    {
        QString name = i.key();
        QString value = i.value();

        name.replace("--", "");
        if (verbose)
            cerr << "name: " << name.toLocal8Bit().constData()
                 << " -- value: " << value.toLocal8Bit().constData() << endl;

        name.append("%");
        name.prepend("%");
        message.replace(name, value);
    }

    if (verbose)
        cout << "output:\n" << message.toLocal8Bit().constData() << endl;

    QUdpSocket *sock = new QUdpSocket();
    QByteArray utf8 = message.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8, address, port) < 0)
    {
        cout << "Failed to send UDP/XML packet" << endl;
    }
    else
    {
        cout << "Sent UDP/XML packet to IP "
             << address.toString().toLocal8Bit().constData()
             << " and port: " << port << endl;
    }

    sock->deleteLater();
   
    return GENERIC_EXIT_OK;
}


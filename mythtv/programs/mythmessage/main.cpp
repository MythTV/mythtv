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

#ifndef VERBOSE
#define VB_IMPORTANT 0
#define VERBOSE(LEVEL, MSG) \
    do { cout << QString(MSG).toLocal8Bit().constData() << endl; } while (0)
#endif

const QString kMessage =
"<mythmessage version=\"1\">\n"
"  <text>%message_text%</text>\n"
"</mythmessage>";

static void printHelp(void)
{
    cout << "\nUsage: mythmessage --message_text=<yourtext> [OPTION]\n";
    cout << "A utility to broadcast messages to all running mythfrontends.\n\n";
    cout << "  --udpport=\"value\"     : (optional) UDP port to sent to (--udpport=6948)\n";
    cout << "  --bcastaddr\"value\"    : (optional) IP address to send to\n";
    cout << "                          (--bcastaddr=127.0.0.1)\n";
    cout << "  --verbose             : (optional) extra debug information\n";
    cout << "  --help                : (optional) this text\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName("mythmessage");

    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    QString message = kMessage;

    bool verbose = false;

    if (a.argc() == 0)
    {
        printHelp();
        return GENERIC_EXIT_OK;
    }

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        QString arg = a.argv()[argpos];

        if (arg.startsWith("-h") || arg.startsWith("--help"))
        {
            printHelp();
            return GENERIC_EXIT_OK;
        }
    }

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        QString arg = a.argv()[argpos];

        if (arg.startsWith("--help") || arg.startsWith("--template"))
            continue;

        if (arg.startsWith("--verbose"))
        {
            verbose = true;
            continue;
        }

        if (arg.startsWith("--udpport"))
        {
            port = arg.section("=", 1).toUShort();
            if (port == 0)
                port = 6948;
            continue;
        }

        if (arg.startsWith("--bcastaddr"))
        {
            QString newaddr = arg.section("=", 1);
            if (!newaddr.isEmpty())
                address.setAddress(newaddr);
            continue;
        }

        QString name = arg.section("=", 0, 0);
        name.replace("--", "");

        QString value = QString::fromLocal8Bit(
            arg.section("=", 1).toLocal8Bit());
        if (verbose)
        {
            QByteArray tmp_name  = name.toLocal8Bit();
            QByteArray tmp_value = value.toLocal8Bit();
            cerr << "name: " << tmp_name.constData()
                 << " -- value: " << tmp_value.constData() << endl;
        }

        name.append("%");
        name.prepend("%");

        message.replace(name, value);
    }

    if (verbose)
    {
        QByteArray tmp_message  = message.toLocal8Bit();
        cout << "output:\n" << tmp_message.constData() << endl;
    }

    QUdpSocket *sock = new QUdpSocket();
    QByteArray utf8 = message.toUtf8();
    int size = utf8.length();

    if (sock->writeDatagram(utf8.constData(), size, address, port) < 0)
    {
        VERBOSE(VB_IMPORTANT, "Failed to send UDP/XML packet");
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


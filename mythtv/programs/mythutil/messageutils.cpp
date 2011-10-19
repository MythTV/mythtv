// C++ includes
#include <iostream>

// Qt headers
#include <QHostAddress>
#include <QUdpSocket>
#include <QString>
#include <QFile>

// libmyth* includes
#include "compat.h"
#include "exitcodes.h"
#include "mythlogging.h"

// Local includes
#include "messageutils.h"

const QString kMessage =
"<mythmessage version=\"1\">\n"
"  <text>%message_text%</text>\n"
"</mythmessage>";

static int PrintTemplate(const MythUtilCommandLineParser &cmdline)
{
    cerr << kMessage.toLocal8Bit().constData() << endl;
    return GENERIC_EXIT_OK;
}

static int SendMessage(const MythUtilCommandLineParser &cmdline)
{
    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    QString message = kMessage;

    if (cmdline.toBool("udpport"))
        port = (unsigned short)cmdline.toUInt("udpport");
    if (cmdline.toBool("bcastaddr"))
        address.setAddress(cmdline.toString("bcastaddr"));

    QMap<QString,QString>::const_iterator i;
    QMap<QString,QString> extras = cmdline.GetExtra();
    for (i = extras.begin(); i != extras.end(); ++i)
    {
        QString name = i.key();
        QString value = i.value();

        name.replace("--", "");
        cerr << "name: " << name.toLocal8Bit().constData()
             << " -- value: " << value.toLocal8Bit().constData() << endl;

        name.append("%");
        name.prepend("%");
        message.replace(name, value);
    }

    cout << "output:\n" << message.toLocal8Bit().constData() << endl;

    QUdpSocket *sock = new QUdpSocket();
    QByteArray utf8 = message.toUtf8();

    int result = GENERIC_EXIT_OK;
    if (sock->writeDatagram(utf8, address, port) < 0)
    {
        cout << "Failed to send UDP/XML packet" << endl;
        result = GENERIC_EXIT_NOT_OK;
    }
    else
    {
        cout << "Sent UDP/XML packet to IP "
             << address.toString().toLocal8Bit().constData()
             << " and port: " << port << endl;
    }

    sock->deleteLater();
   
    return result;
}

void registerMessageUtils(UtilMap &utilMap)
{
    utilMap["message"]                 = &SendMessage;
    utilMap["printtemplate"]           = &PrintTemplate;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

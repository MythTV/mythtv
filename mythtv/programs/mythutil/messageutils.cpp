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
"  <timeout>%timeout%</timeout>\n"
"</mythmessage>";

const QString kNotification =
"<mythnotification version=\"1\">\n"
"  <text>%message_text%</text>\n"
"  <timeout>%timeout%</timeout>\n"
"</mythnotification>";

static int PrintMTemplate(const MythUtilCommandLineParser &cmdline)
{
    cerr << kMessage.toLocal8Bit().constData() << endl;
    return GENERIC_EXIT_OK;
}

static int PrintNTemplate(const MythUtilCommandLineParser &cmdline)
{
    cerr << kNotification.toLocal8Bit().constData() << endl;
    return GENERIC_EXIT_OK;
}

static int SendMessage(const MythUtilCommandLineParser &cmdline)
{
    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;
    QString name = cmdline.GetPassthrough();
    bool notification = cmdline.toBool("notification");
    QString text = "message";
    QString timeout = "0";

    QString message = notification ? kNotification : kMessage;

    if (cmdline.toBool("udpport"))
        port = (unsigned short)cmdline.toUInt("udpport");
    if (cmdline.toBool("bcastaddr"))
        address.setAddress(cmdline.toString("bcastaddr"));
    if (cmdline.toBool("message_text"))
        text = cmdline.toString("message_text");
    if (cmdline.toBool("timeout"))
        timeout = cmdline.toString("timeout");
    message.replace("%message_text%", text);
    message.replace("%timeout%", timeout);

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
    utilMap["printmtemplate"]          = &PrintMTemplate;
    utilMap["notification"]            = &SendMessage;
    utilMap["printntemplate"]          = &PrintNTemplate;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

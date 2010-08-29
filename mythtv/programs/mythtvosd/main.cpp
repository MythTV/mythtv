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

const QString kalert =
"<mythnotify version=\"1\">\n"
"  <container name=\"notify_alert_text\">\n"
"    <textarea name=\"notify_text\">\n"
"      <value>%alert_text%</value>\n"
"    </textarea>\n"
"  </container>\n"
"</mythnotify>";

const QString kscroll =
"<mythnotify version=\"1\" displaytime=\"-1\">\n"
"  <container name=\"news_scroller\">\n"
"    <textarea name=\"text_scroll\">\n"
"      <value>%scroll_text%</value>\n"
"    </textarea>\n"
"  </container>\n"
"</mythnotify>";

const QString kcid = 
"<mythnotify version=\"1\">\n"
"  <container name=\"notify_cid_info\">\n"
"    <textarea name=\"notify_cid_line\">\n"
"      <value>LINE #%caller_line%</value>\n"
"    </textarea>\n"
"    <textarea name=\"notify_cid_name\">\n"
"      <value>NAME: %caller_name%</value>\n"
"    </textarea>\n"
"    <textarea name=\"notify_cid_num\">\n"
"      <value>NUM : %caller_number%</value>\n"
"    </textarea>\n"
"    <textarea name=\"notify_cid_dt\">\n"
"      <value>DATE: %caller_date% TIME : %caller_time%</value>\n"
"    </textarea>\n"
"  </container>\n"
"</mythnotify>";

static void printHelp(void)
{
    cout << "\nUsage: mythtvosd --template=<name> [OPTION]\n";
    cout << "A utility to put items on the mythTV OSD.\n\n";
    cout << "  --template=<name>     : (required) template name or xml file to send\n";
    cout << "                          (--file=cid.xml)\n";
    cout << "                          Valid template names are 'cid', 'alert',\n";
    cout << "                          and 'scroller'\n";
    cout << "  --replace=\"value\"     : (optional) 'replace' can be any word present in the\n";
    cout << "                          template\n";
    cout << "                        : 'cid' has: caller_line, caller_name, caller_number\n";
    cout << "                                     caller_date, and caller_time\n";
    cout << "                        : 'alert' has: alert_text\n";
    cout << "                        : 'scroller' has: scroll_text\n";
    cout << "  --udpport=\"value\"     : (optional) UDP port to sent to (--udpport=6948)\n";
    cout << "  --bcastaddr\"value\"    : (optional) IP address to send to\n";
    cout << "                          (--bcastaddr=127.0.0.1)\n";
    cout << "  --verbose             : (optional) extra debug information\n";
    cout << "  --help                : (optional) this text\n";

    cout << "\nAn XML file or template name is required - it will be read into memory. Then\n";
    cout << "each %%name%% item will be replaced with the given 'value'.\n";
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QHostAddress address = QHostAddress::Broadcast;
    unsigned short port = 6948;

    QString message = "";

    bool verbose = false;

    QString templatearg = "";

    if (a.argc() == 0)
    {
        printHelp();
        return TVOSD_EXIT_OK;
    }

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        QString arg = a.argv()[argpos];

        if (arg.startsWith("-h") || arg.startsWith("--help"))
        {
            printHelp();
            return TVOSD_EXIT_OK;
        }

        if (arg.startsWith("--template="))
        {
            templatearg = arg.section('=', 1);
            break;
        }
    }

    if (templatearg.isEmpty())
    {
        cerr << "--template=<value> argument is required\n"
             << "  <value> can be 'alert', 'cid', 'scroller', or a "
             << "filename of a template\n";
        return TVOSD_EXIT_INVALID_CMDLINE;
    }

    if (templatearg == "cid")
        message = kcid;
    else if (templatearg == "alert")
        message = kalert;
    else if (templatearg == "scroller")
        message = kscroll;
    else
    {
        QFile mfile(templatearg);
        if (!mfile.open(QIODevice::ReadOnly))
        {
            cerr << "Unable to open template file '"
                 << templatearg.toLocal8Bit().constData() << "'\n";
            return TVOSD_EXIT_NO_TEMPLATE;
        }

        QByteArray mdata = mfile.readAll();
        message = QString(mdata);
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
   
    return TVOSD_EXIT_OK;
}


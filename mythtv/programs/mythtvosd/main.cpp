#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>

#include <iostream>

#include <qapplication.h>
#include <qsocketdevice.h>
#include <qstring.h>
#include <qcstring.h>
#include <qfile.h>
#include <qhostaddress.h>

#include "exitcodes.h"

using namespace std;

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

void printHelp(void)
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
    QApplication a(argc, argv, false);

    QHostAddress address;
    address.setAddress("255.255.255.255");
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

    if (templatearg == "")
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
        if (!mfile.open(IO_ReadOnly))
        {
            cerr << "Unable to open template file '" << templatearg << "'\n";
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

        QString value = arg.section("=", 1);
        if (verbose)
        {

            cerr << "name: " << name << " -- value: " << value << endl;
        }

        name.append("%");
        name.prepend("%");

        message.replace(name, value);
    }

    if (verbose)
        cout << "output:\n" << message << endl;

    QSocketDevice sock(QSocketDevice::Datagram);

    int yes = 1;
    if (setsockopt(sock.socket(), SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int))
        < 0)
    {
        perror("Set broadcast");
        return TVOSD_EXIT_SOCKET_ERROR;
    }

    QCString utf8 = message.utf8();
    int size = utf8.length();

    if (sock.writeBlock(utf8.data(), size, address, port) < 0)
    {
        perror("sendto");
    }
    else
    {
        cout << "Sent UDP/XML packet to IP " << address.toString() 
             << " and port: " << port << endl;
    }
   
    return TVOSD_EXIT_OK;
}


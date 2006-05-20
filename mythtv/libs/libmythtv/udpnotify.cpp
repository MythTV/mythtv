/*
UDPNotify - 9/13/2003
Ken Bass

This code receives XML data via a UDP socket. The XML data is parsed
to extract the appropriate container/textarea names. Then the OSD
class is used to display the values provided in the XML for those
widgets.

As an example, the input XML should look like:

<mythnotify version="1">
  <container name="notify_cid_info">
    <textarea name="notify_cid_name">
      <value>NAME: JOE SCHMOE</value>
    </textarea>
    <textarea name="notify_cid_num">
      <value>NUM : 301-555-1212</value>
    </textarea>
  </container>
</mythnotify>

The container and textarea widget names in the XML above must match
the corresponding widgets defined within the osd.xml file. If they do not
match they will be ignored.
*/


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <qapplication.h>
#include <qsocketdevice.h>
#include <qsocketnotifier.h>
#include <qhostaddress.h>

#include <iostream>
using namespace std;

#include "udpnotify.h"
#include "mythcontext.h"
#include "osd.h"
#include "tv_play.h"

UDPNotifyOSDSet::UDPNotifyOSDSet(const QString &name)
{
    m_name = name;

    allTypes = new vector<UDPNotifyOSDTypeText *>;
}

UDPNotifyOSDSet::~UDPNotifyOSDSet()
{
    vector<UDPNotifyOSDTypeText *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        UDPNotifyOSDTypeText *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
}

UDPNotifyOSDTypeText *UDPNotifyOSDSet::GetType(const QString &name)
{
    UDPNotifyOSDTypeText *ret = NULL;
    if (typesMap.contains(name))
        ret = typesMap[name];

    return ret;
}

void UDPNotifyOSDSet::ResetTypes(void)
{
    typesMap.clear();
    allTypes->clear();
}

void UDPNotifyOSDSet::AddType(UDPNotifyOSDTypeText *type, QString name)
{
    typesMap[name] = type;
    allTypes->push_back(type);
}

QString UDPNotifyOSDSet::GetName(void)
{
    return m_name;
}

vector<UDPNotifyOSDTypeText *> *UDPNotifyOSDSet::GetTypeList()
{
    return allTypes;
}

UDPNotifyOSDTypeText::UDPNotifyOSDTypeText(const QString &name, 
                                           const QString &text)
{
    m_name = name;
    m_text = text;
}

UDPNotifyOSDTypeText::~UDPNotifyOSDTypeText()
{
}

QString UDPNotifyOSDTypeText::GetName(void)
{
    return m_name;
}

QString UDPNotifyOSDTypeText::GetText(void)
{
    return m_text;
}

void UDPNotifyOSDTypeText::SetText(const QString &text)
{
    m_text = text;
}

UDPNotify::UDPNotify(TV *tv, int udp_port)
         : QObject()
{
    m_tv = tv;
    setList = new vector<UDPNotifyOSDSet *>;

    // Address to listen to - listen on all interfaces
    bcastaddr.setAddress("0.0.0.0");
  
    // Setup UDP receive socket and install notifier
    m_udp_port = udp_port;

    // need to lock because of the socket notifier.
    qApp->lock();

    qsd = new QSocketDevice(QSocketDevice::Datagram);
    if (!qsd->bind(bcastaddr, udp_port))
    {
        VERBOSE(VB_IMPORTANT, QString("Could not bind to UDP notify port: %1")
                                       .arg(udp_port));
        qsn = NULL;
    }
    else
    {
        // Create the notifier
        qsn = new QSocketNotifier(qsd->socket(), QSocketNotifier::Read);

        // Connect the Notifier to the incming data slot
        connect(qsn, SIGNAL(activated(int)), this, SLOT(incomingData(int)));
    }

    qApp->unlock();
}

UDPNotify::~UDPNotify(void)
{
    qApp->lock();

    disconnect(qsn, SIGNAL(activated(int)), this, SLOT(incomingData(int)));

    qsd->close();

    delete qsd;

    if (qsn)
        delete qsn;

    qApp->unlock();

    vector<UDPNotifyOSDSet *>::iterator i = setList->begin();
    for (; i != setList->end(); i++)
    {
        UDPNotifyOSDSet *set = (*i);
        if (set)
            delete set;
    }
    delete setList;
}

void UDPNotify::AddSet(UDPNotifyOSDSet *set, QString name)
{
    setMap[name] = set;
    setList->push_back(set);
}

UDPNotifyOSDSet *UDPNotify::GetSet(const QString &text)
{
    UDPNotifyOSDSet *ret = NULL;
    if (setMap.contains(text))
        ret = setMap[text];

    return ret;
}

QString UDPNotify::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

void UDPNotify::parseTextArea(UDPNotifyOSDSet *container, QDomElement &element)
{
    QString value;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Text area needs a name");
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "value")
            {
                value  = getFirstText(info);

                UDPNotifyOSDTypeText *text = container->GetType(name);
                if (text != NULL)
                {
                    text->SetText(value);
                }
                else
                {
                    text = new UDPNotifyOSDTypeText(name, value);
                    container->AddType(text, name);
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown tag in text area: %1")
                                       .arg(info.tagName()));
            }                   
        }
    }    
}

UDPNotifyOSDSet *UDPNotify::parseContainer(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Container needs a name");
        return NULL;
    }

    UDPNotifyOSDSet *container = GetSet(name);

    if (container != NULL)
    {
        ClearContainer(container);
    }
    else
    {
        container = new UDPNotifyOSDSet(name);
        AddSet(container, name);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown container child: %1")
                                       .arg(info.tagName()));
            }
        }
    }

    return container;
}

void UDPNotify::ClearContainer(UDPNotifyOSDSet *container)
{
    OSD *osd = m_tv->GetOSD();

    if (osd)
        osd->ClearNotify(container);
    container->ResetTypes();
}

void UDPNotify::incomingData(int socket)
{
    OSD *osd = m_tv->GetOSD();
    QByteArray buf;
    int nr;

    socket = socket;

    // Read the data
    buf.resize(qsd->bytesAvailable());
    nr = qsd->readBlock(buf.data(), qsd->bytesAvailable()); 
    if (nr < 0)
    {
        VERBOSE(VB_IMPORTANT, "Error reading from udpnotify socket");
        return;
    }
    buf.resize(nr);  // Resize to actual bytes read
    //VERBOSE(VB_IMPORTANT, QString("Read %1 bytes from peer IP %2 port %3")
    //                       .arg(nr)
    //                       .arg(qsd->peerAddress().toString())
    //                       .arg(qsd->port()));

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
  
    if (!doc.setContent(buf, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("Error parsing udpnotify xml:\n"
                                "at line: %1  column: %2\n%3")
                               .arg(errorLine)
                               .arg(errorColumn)
                               .arg(errorMsg));
        return;
    }
 
    int displaytime = gContext->GetNumSetting("OSDNotifyTimeout", 5);
 
    QDomElement docElem = doc.documentElement();
    if (!docElem.isNull())
    {
        if (docElem.tagName() != "mythnotify")
        {
            VERBOSE(VB_IMPORTANT, "Unknown UDP packet (not <mythnotify> XML)");
            return;
        }

        QString version = docElem.attribute("version", "");
        if (version.isNull() || version.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "<mythnotify> missing 'version' attribute");
            return;
        }

        QString disptime = docElem.attribute("displaytime", "");
        if (!disptime.isNull() && !disptime.isEmpty())
            displaytime = disptime.toInt();
    }

    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "container")
            {
                UDPNotifyOSDSet *container = parseContainer(e);
                if (osd && container)
                    osd->StartNotify(container, displaytime);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown element: %1")
                                       .arg(e.tagName()));
                return;
            }
        }
        n = n.nextSibling();
    }
}


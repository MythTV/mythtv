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

#include <QUdpSocket>
#include <QDomDocument>

#include "udpnotify.h"
#include "mythcorecontext.h"
#include "mythverbose.h"

UDPNotifyOSDSet::UDPNotifyOSDSet(const QString &name, uint timeout)
    : m_name(name), m_timeout(timeout)
{
    m_name.detach();
}

void UDPNotifyOSDSet::ResetTypes(void)
{
    QMutexLocker locker(&m_lock);
    m_typesMap.clear();
}

void UDPNotifyOSDSet::SetType(const QString &name, const QString &value)
{
    QMutexLocker locker(&m_lock);
    QString tmp_name  = name;  tmp_name.detach();
    QString tmp_value = value; tmp_value.detach();
    m_typesMap[tmp_name] = tmp_value;
}

QString UDPNotifyOSDSet::GetName(void) const
{
    QMutexLocker locker(&m_lock);
    QString tmp = m_name; tmp.detach();
    return tmp;
}

uint UDPNotifyOSDSet::GetTimeout(void) const
{
    QMutexLocker locker(&m_lock);
    return m_timeout;
}

void UDPNotifyOSDSet::SetTimeout(uint timeout_in_seconds)
{
    QMutexLocker locker(&m_lock);
    m_timeout = timeout_in_seconds;
}

/////////////////////////////////////////////////////////////////////////

UDPNotify::UDPNotify(uint udp_port) :
    m_socket(new QUdpSocket()), m_db_osd_udpnotify_timeout(5)
{
    connect(m_socket, SIGNAL(readyRead()),
            this,     SLOT(ReadPending()));

    m_socket->bind(udp_port);
}

void UDPNotify::deleteLater(void)
{
    TeardownAll();
    disconnect();
    QObject::deleteLater();
}

void UDPNotify::TeardownAll(void)
{
    if (m_socket)
    {
        m_socket->disconnect();
        m_socket->close();
        m_socket->deleteLater();
        m_socket = NULL;
    }

    emit ClearUDPNotifyEvents();

    UDPNotifyOSDSetMap::iterator it = m_sets.begin();
    for (; it != m_sets.end(); ++it)
    {
        delete *it;
    }
    m_sets.clear();
}

QString UDPNotify::GetFirstText(QDomElement &element)
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

void UDPNotify::ParseTextArea(UDPNotifyOSDSet *container, QDomElement &element)
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
                value  = GetFirstText(info);

                container->SetType(name, value);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown tag in text area: %1")
                                       .arg(info.tagName()));
            }
        }
    }
}

UDPNotifyOSDSet *UDPNotify::ParseContainer(QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Container needs a name");
        return NULL;
    }

    UDPNotifyOSDSetMap::iterator it = m_sets.find(name);
    if (it == m_sets.end())
    {
        it = m_sets.insert(name, new UDPNotifyOSDSet(
                               name, m_db_osd_udpnotify_timeout));
    }
    else
    {
        ClearContainer(*it);
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "textarea")
            {
                ParseTextArea(*it, info);
            }
            else
            {
                VERBOSE(VB_IMPORTANT, QString("Unknown container child: %1")
                        .arg(info.tagName()));
            }
        }
    }

    return *it;
}

void UDPNotify::ClearContainer(UDPNotifyOSDSet *container)
{
    container->ResetTypes();
    emit AddUDPNotifyEvent(container->GetName(), NULL);
}

void UDPNotify::ReadPending(void)
{
    QByteArray buf;
    while (m_socket->hasPendingDatagrams())
    {
        buf.resize(m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(buf.data(), buf.size(),
                               &sender, &senderPort);

        Process(buf);
    }
}

void UDPNotify::Process(const QByteArray &buf)
{
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    QDomDocument doc;
    if (!doc.setContent(buf, false, &errorMsg, &errorLine, &errorColumn))
    {
        VERBOSE(VB_IMPORTANT, QString("UDPNotify, Error: ") +
                QString("Parsing udpnotify xml:\n\t\t\t"
                        "at line: %1  column: %2\n\t\t\t%3")
                .arg(errorLine).arg(errorColumn).arg(errorMsg));

        return;
    }

    int displaytime = -1;

    QDomElement docElem = doc.documentElement();
    if (!docElem.isNull())
    {
        if (docElem.tagName() != "mythnotify")
        {
            VERBOSE(VB_IMPORTANT, "Unknown UDP packet (not <mythnotify> XML)");
            return;
        }

        QString version = docElem.attribute("version", "");
        if (version.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "<mythnotify> missing 'version' attribute");
            return;
        }

        QString disptime = docElem.attribute("displaytime", "");
        if (!disptime.isEmpty())
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
                UDPNotifyOSDSet *container = ParseContainer(e);
                if (displaytime >= 0)
                    container->SetTimeout(displaytime);
                emit AddUDPNotifyEvent(container->GetName(), container);
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

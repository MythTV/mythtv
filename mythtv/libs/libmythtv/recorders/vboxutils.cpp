#include <chrono> // for milliseconds
#include <thread> // for sleep_for

// Qt
#include <QString>
#include <QStringList>
#include <QDomDocument>
#include <QRegExp>

// MythTV headers
#include "vboxutils.h"
#include "mythdownloadmanager.h"
#include "mythlogging.h"
#include "ssdp.h"
#include "mythtimer.h"

#define LOC QString("VBox: ")

#define QUERY_BOARDINFO "http://{URL}/cgi-bin/HttpControl/HttpControlApp?OPTION=1&Method=QueryBoardInfo"
#define QUERY_CHANNELS  "http://{URL}/cgi-bin/HttpControl/HttpControlApp?OPTION=1&Method=GetXmltvChannelsList"\
                        "&FromChIndex=FirstChannel&ToChIndex=LastChannel&FilterBy=All"

#define SEARCH_TIME 3000
#define VBOX_URI "urn:schemas-upnp-org:device:MediaServer:1"
#define VBOX_UDN "uuid:b7531642-0123-3210"

VBox::VBox(const QString &url)
{
    m_url = url;
}

// static method
QStringList VBox::probeDevices(void)
{
    const int milliSeconds = SEARCH_TIME;

    // see if we have already found one or more vboxes
    QStringList result = VBox::doUPNPSearch();

    if (result.count())
        return result;

    // non found so start a new search
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using UPNP to search for Vboxes (%1 secs)")
        .arg(milliSeconds / 1000));

    SSDP::Instance()->PerformSearch(VBOX_URI, milliSeconds / 1000);

    // Search for a total of 'milliSeconds' ms, sending new search packet
    // about every 250 ms until less than one second remains.
    MythTimer totalTime; totalTime.start();
    MythTimer searchTime; searchTime.start();
    while (totalTime.elapsed() < milliSeconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        int ttl = milliSeconds - totalTime.elapsed();
        if ((searchTime.elapsed() > 249) && (ttl > 1000))
        {
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UPNP Search %1 secs")
                .arg(ttl / 1000));
            SSDP::Instance()->PerformSearch(VBOX_URI, ttl / 1000);
            searchTime.start();
        }
    }

    return VBox::doUPNPSearch();
}

QStringList VBox::doUPNPSearch(void)
{
    QStringList result;

    SSDPCacheEntries *vboxes = SSDP::Find(VBOX_URI);

    if (!vboxes)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "No UPnP VBoxes found");
        return QStringList();
    }

    int count = vboxes->Count();
    if (count)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Found %1 possible VBoxes").arg(count));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "No UPnP VBoxes found, but SSDP::Find() not NULL");
    }

    EntryMap map;
    vboxes->GetEntryMap(map);

    foreach (auto BE, map)
    {
        if (!BE->GetDeviceDesc())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("GetDeviceDesc() failed for %1").arg(BE->GetFriendlyName()));
            continue;
        }

        QString friendlyName = BE->GetDeviceDesc()->m_rootDevice.m_sFriendlyName;
        QString ip = BE->GetDeviceDesc()->m_hostUrl.host();
        QString udn = BE->GetDeviceDesc()->m_rootDevice.m_sUDN;
        int port = BE->GetDeviceDesc()->m_hostUrl.port();

        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("Found possible VBox at %1 (%2:%3)").arg(friendlyName).arg(ip).arg(port));

        if (udn.startsWith(VBOX_UDN))
        {
            // we found one
            QString id;
            int startPos = friendlyName.indexOf('(');
            int endPos = friendlyName.indexOf(')');

            if (startPos != -1 && endPos != -1)
                id = friendlyName.mid(startPos + 1, endPos - startPos - 1);
            else
                id = friendlyName;

            // get a list of tuners on this VBOX
            QStringList tuners;

            VBox *vbox = new VBox(ip);
            tuners = vbox->getTuners();
            delete vbox;

            for (int x = 0; x < tuners.count(); x++)
            {
                // add a device in the format ID IP TUNERNO TUNERTYPE
                // eg vbox_3718 192.168.1.204 1 DVBT/T2
                const QString& tuner = tuners.at(x);
                QString device = QString("%1 %2 %3").arg(id).arg(ip).arg(tuner);
                result << device;
                LOG(VB_GENERAL, LOG_INFO, QString("Found VBox - %1").arg(device));
            }
        }

        BE->DecrRef();
    }

    vboxes->DecrRef();
    vboxes = nullptr;

    return result;
}

// static method
QString VBox::getIPFromVideoDevice(const QString& dev)
{
    // dev is of the form xx.xx.xx.xx-n-t or xxxxxxx-n-t
    // where xx is either an ip address or vbox id
    // n is the tuner number and t is the tuner type ie DVBT/T2
    QStringList devItems = dev.split("-");

    if (devItems.size() != 3)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Got malformed videodev %1").arg(dev));
        return QString();
    }

    QString id = devItems.at(0).trimmed();

    // if we already have an ip address use that
    QRegExp ipRegExp("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
    if (id.indexOf(ipRegExp) == 0)
        return id;

    // we must have a vbox id so look it up to find the ip address
    QStringList vboxes = VBox::probeDevices();

    for (int x = 0; x < vboxes.count(); x++)
    {
        QStringList vboxItems = vboxes.at(x).split(" ");
        if (vboxItems.size() != 4)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Got malformed probed device %1").arg(vboxes.at(x)));
            continue;
        }

        const QString& vboxID = vboxItems.at(0);
        QString vboxIP = vboxItems.at(1);

        if (vboxID == id)
            return vboxIP;
    }

    // if we get here we didn't find it
    return QString();
}

QDomDocument *VBox::getBoardInfo(void)
{
    auto *xmlDoc = new QDomDocument();
    QString query = QUERY_BOARDINFO;

    query.replace("{URL}", m_url);

    if (!sendQuery(query, xmlDoc))
    {
        delete xmlDoc;
        return nullptr;
    }

    return xmlDoc;
}

bool VBox::checkConnection(void)
{
    // assume if we can download the board info we have a good connection
    return (getBoardInfo() != nullptr);
}

bool VBox::checkVersion(QString &version)
{
    QString requiredVersion = VBOX_MIN_API_VERSION;
    QStringList sList = requiredVersion.split('.');

    // sanity check this looks like a VBox version string
    if (sList.count() < 3 || !requiredVersion.startsWith("V"))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Failed to parse required version from %1").arg(requiredVersion));
        version = "UNKNOWN";
        return false;
    }

    int requiredMajor = sList[1].toInt();
    int requiredMinor = sList[2].toInt();

    int major = 0;
    int minor = 0;

    QDomDocument *xmlDoc = getBoardInfo();
    QDomElement elem = xmlDoc->documentElement();

    if (!elem.isNull())
    {
        version = getStrValue(elem, "SoftwareVersion");

        sList = version.split('.');

        // sanity check this looks like a VBox version string
        if (sList.count() < 3 || !(version.startsWith("VB.") || version.startsWith("VJ.")
            || version.startsWith("VT.")))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Failed to parse version from %1").arg(version));
            delete xmlDoc;
            return false;
        }

        major = sList[1].toInt();
        minor = sList[2].toInt();
    }

    delete xmlDoc;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("CheckVersion - required: %1, actual: %2").arg(VBOX_MIN_API_VERSION).arg(version));

    if (major < requiredMajor)
        return false;

    if (major == requiredMajor && minor < requiredMinor)
        return false;

    return true;
}

/// returns a list of tuners in the format 'TUNERNO TUNERTYPE' eg '1 DVBT/T2'
QStringList VBox::getTuners(void)
{
    QStringList result;

    QDomDocument *xmlDoc = getBoardInfo();
    QDomElement elem = xmlDoc->documentElement();

    if (!elem.isNull())
    {
        int noTuners = getIntValue(elem, "TunersNumber");

        for (int x = 1; x <= noTuners; x++)
        {
            QString tuner = getStrValue(elem, QString("Tuner%1").arg(x));
            QString s = QString("%1 %2").arg(x).arg(tuner);
            result.append(s);
        }
    }

    delete xmlDoc;

    return result;
}


vbox_chan_map_t *VBox::getChannels(void)
{
    auto *result = new vbox_chan_map_t;
    auto *xmlDoc = new QDomDocument();
    QString query = QUERY_CHANNELS;

    query.replace("{URL}", m_url);

    if (!sendQuery(query, xmlDoc))
    {
        delete xmlDoc;
        delete result;
        return nullptr;
    }

    QDomNodeList chanNodes = xmlDoc->elementsByTagName("channel");

    for (int x = 0; x < chanNodes.count(); x++)
    {
        QDomElement chanElem = chanNodes.at(x).toElement();
        QString xmltvid = chanElem.attribute("id", "UNKNOWN_ID");
        QString name = getStrValue(chanElem, "display-name", 0);
        QString chanType = getStrValue(chanElem, "display-name", 1);
        QString triplet = getStrValue(chanElem, "display-name", 2);
        bool    fta = (getStrValue(chanElem, "display-name", 3) == "Free");
        QString lcn = getStrValue(chanElem, "display-name", 4);
        uint serviceID = triplet.right(4).toUInt(nullptr, 16);

        QString transType = "UNKNOWN";
        QStringList slist = triplet.split('-');
        uint networkID = slist[2].left(4).toUInt(nullptr, 16);
        uint transportID = slist[2].mid(4, 4).toUInt(nullptr, 16);
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("NIT/TID/SID %1 %2 %3)").arg(networkID).arg(transportID).arg(serviceID));

        //sanity check - the triplet should look something like this: T-GER-111100020001
        // where T is the tuner type, GER is the country, and the numbers are the NIT/TID/SID
        if (slist.count() == 3)
            transType = slist[0];

        QString icon = "";
        QDomNodeList iconNodes = chanElem.elementsByTagName("icon");
        if (iconNodes.count())
        {
            QDomElement iconElem = iconNodes.at(0).toElement();
            icon = iconElem.attribute("src", "");
        }

        QString url = "";
        QDomNodeList urlNodes = chanElem.elementsByTagName("url");
        if (urlNodes.count())
        {
            QDomElement urlElem = urlNodes.at(0).toElement();
            url = urlElem.attribute("src", "");
        }

        VBoxChannelInfo chanInfo(name, xmltvid, url, fta, chanType, transType, serviceID, networkID, transportID);
        result->insert(lcn, chanInfo);
    }

    return result;
}

bool VBox::sendQuery(const QString& query, QDomDocument* xmlDoc)
{
    QByteArray result;

    if (!GetMythDownloadManager()->download(query, &result, true))
        return false;

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!xmlDoc->setContent(result, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Error parsing: %1\nat line: %2  column: %3 msg: %4").
                arg(query).arg(errorLine).arg(errorColumn).arg(errorMsg));
        return false;
    }

    // check for a status or error element
    QDomNodeList statusNodes = xmlDoc->elementsByTagName("Status");

    if (!statusNodes.count())
        statusNodes = xmlDoc->elementsByTagName("Error");

    if (statusNodes.count())
    {
        QDomElement elem = statusNodes.at(0).toElement();
        if (!elem.isNull())
        {
            ErrorCode errorCode = (ErrorCode)getIntValue(elem, "ErrorCode");
            QString errorDesc = getStrValue(elem, "ErrorDescription");

            if (errorCode == SUCCESS)
                return true;

            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("API Error: %1 - %2, Query was: %3").arg(errorCode).arg(errorDesc).arg(query));

            return false;
        }
    }

    // no error detected so assume we got a valid xml result
    return true;
}

QString VBox::getStrValue(QDomElement &element, const QString &name, int index)
{
    QDomNodeList nodes = element.elementsByTagName(name);
    if (!nodes.isEmpty())
    {
        if (index >= nodes.count())
            index = 0;
        QDomElement e = nodes.at(index).toElement();
        return getFirstText(e);
    }

    return QString();
}

int VBox::getIntValue(QDomElement &element, const QString &name, int index)
{
    QString value = getStrValue(element, name, index);

    return value.toInt();
}

QString VBox::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return QString();
}

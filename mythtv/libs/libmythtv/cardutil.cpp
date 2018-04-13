// Standard UNIX C headers
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#if defined(USING_V4L2) || defined(USING_DVB)
#include <sys/ioctl.h>
#endif

// Qt headers
#include <QMap>
#include <QDir>

// MythTV headers
#include "mythconfig.h"
#include "cardutil.h"
#include "videosource.h"
#include "dvbchannel.h"
#include "diseqcsettings.h"
#include "sourceutil.h"
#include "mythdb.h"
#include "mythlogging.h"
#include "inputinfo.h"
#include "mythmiscutil.h" // for ping()
#include "mythdownloadmanager.h"

#ifdef USING_DVB
#include "dvbtypes.h"
#endif

#ifdef USING_V4L1
#include <linux/videodev.h>
#endif

#ifdef USING_V4L2
#include "v4l2util.h"
#endif

#ifdef USING_HDHOMERUN
#include "hdhomerun.h"
#endif

#ifdef USING_VBOX
#include "vboxutils.h"
#include "mythmiscutil.h"
#endif

#ifdef USING_ASI
#include <sys/types.h>
#include <sys/stat.h>
#include <dveo/asi.h>
#include <dveo/master.h>
#endif

#define LOC      QString("CardUtil: ")

QString CardUtil::GetScanableInputTypes(void)
{
    QString inputTypes = "";

#ifdef USING_DVB
    inputTypes += "'DVB'";
#endif // USING_DVB

#ifdef USING_V4L2
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'V4L'";
# ifdef USING_IVTV
    inputTypes += ",'MPEG'";
# endif // USING_IVTV
#endif // USING_V4L2

#ifdef USING_IPTV
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'FREEBOX'";
#endif // USING_IPTV

#ifdef USING_VBOX
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'VBOX'";
#endif // USING_VBOX

#ifdef USING_HDHOMERUN
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'HDHOMERUN'";
#endif // USING_HDHOMERUN

#ifdef USING_ASI
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'ASI'";
#endif

#ifdef USING_CETON
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'CETON'";
#endif // USING_CETON

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    if (!inputTypes.isEmpty())
        inputTypes += ",";
    inputTypes += "'EXTERNAL'";
#endif

    if (inputTypes.isEmpty())
        inputTypes = "'DUMMY'";

    return QString("(%1)").arg(inputTypes);
}

bool CardUtil::IsCableCardPresent(uint inputid,
                                  const QString &inputType)
{
    if (inputType == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        hdhomerun_device_t *hdhr;
        hdhomerun_tuner_status_t status;
        QString device = GetVideoDevice(inputid);
        hdhr = hdhomerun_device_create_from_str(device.toLatin1(), NULL);
        if (!hdhr)
            return false;

        int oob = -1;
        oob = hdhomerun_device_get_oob_status(hdhr, NULL, &status);

        // if no OOB tuner, oob will be < 1.  If no CC present, OOB
        // status will be "none."
        if (oob > 0 && (strncmp(status.channel, "none", 4) != 0))
        {
            LOG(VB_GENERAL, LOG_INFO, "Cardutil: HDHomeRun Cablecard Present.");
            hdhomerun_device_destroy(hdhr);
            return true;
        }

        hdhomerun_device_destroy(hdhr);

#endif
        return false;
    }
    else if (inputType == "CETON")
    {
#ifdef USING_CETON
        QString device = GetVideoDevice(inputid);

        QStringList parts = device.split("-");
        if (parts.size() != 2)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("CardUtil: Ceton invalid device id %1").arg(device));
            return false;
        }

        QString ip_address = parts.at(0);

        QStringList tuner_parts = parts.at(1).split(".");
        if (tuner_parts.size() != 2)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CardUtil: Ceton invalid device id %1").arg(device));
            return false;
        }

        uint tuner = tuner_parts.at(1).toUInt();

        QUrlQuery params;
        params.addQueryItem("i", QString::number(tuner));
        params.addQueryItem("s", "cas");
        params.addQueryItem("v", "CardStatus");

        QUrl url;
        url.setScheme("http");
        url.setHost(ip_address);
        url.setPath("/get_var.json");
        url.setQuery(params);

        QNetworkRequest *request = new QNetworkRequest();
        request->setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                              QNetworkRequest::AlwaysNetwork);
        request->setUrl(url);

        QByteArray data;
        MythDownloadManager *manager = GetMythDownloadManager();

        if (!manager->download(request, &data))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("CardUtil: Ceton http request failed %1").arg(device));
            return false;
        }

        QString response = QString(data);

        QRegExp regex("^\\{ \"?result\"?: \"(.*)\" \\}$");
        if (regex.indexIn(response) == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("CardUtil: Ceton unexpected http response: %1").arg(response));
            return false;
        }

        QString result = regex.cap(1);

        if (result == "Inserted")
        {
            LOG(VB_GENERAL, LOG_DEBUG, "Cardutil: Ceton CableCARD present.");
            return true;
        }

        LOG(VB_GENERAL, LOG_DEBUG, "Cardutil: Ceton CableCARD not present.");
        return false;
#else
        return false;
#endif
    }
    else
        return false;
}

bool CardUtil::HasTuner(const QString &rawtype, const QString & device)
{
    if (rawtype == "DVB"     || rawtype == "HDHOMERUN" ||
        rawtype == "FREEBOX" || rawtype == "CETON" || rawtype == "VBOX")
        return true;

#ifdef USING_V4L2
    if (rawtype == "V4L2ENC")
    {
        V4L2util v4l2(device);
        return !v4l2 ? false : v4l2.HasTuner();
    }
#else
    Q_UNUSED(device);
#endif

    if (rawtype == "EXTERNAL")
    {
        // TODO: query EXTERNAL for capability
        return true;
    }

    return false;
}

bool CardUtil::IsTunerShared(uint inputidA, uint inputidB)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("IsTunerShared(%1,%2)")
            .arg(inputidA).arg(inputidB));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, hostname, cardtype "
                  "FROM capturecard "
                  "WHERE ( (cardid = :INPUTID_A) OR "
                  "        (cardid = :INPUTID_B) )");
    query.bindValue(":INPUTID_A", inputidA);
    query.bindValue(":INPUTID_B", inputidB);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::is_tuner_shared", query);
        return false;
    }

    if (!query.next())
        return false;

    const QString vdevice  = query.value(0).toString();
    const QString hostname = query.value(1).toString();
    const QString inputtype = query.value(2).toString();

    if (!IsTunerSharingCapable(inputtype.toUpper()))
        return false;

    if (!query.next())
        return false;

    bool ret = ((vdevice  == query.value(0).toString()) &&
                (hostname == query.value(1).toString()) &&
                (inputtype == query.value(2).toString()));

    LOG(VB_RECORD, LOG_DEBUG, QString("IsTunerShared(%1,%2) -> %3")
            .arg(inputidA).arg(inputidB).arg(ret));

    return ret;
}

/** \fn CardUtil::IsInputTypePresent(const QString&, QString)
 *  \brief Returns true if the input type is present and connected to an input
 *  \param rawtype  Input type as used in DB or empty string for all inputs
 *  \param hostname Host to check, or empty string for current host
 */
bool CardUtil::IsInputTypePresent(const QString &rawtype, QString hostname)
{
    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT count(cardtype) "
        "FROM capturecard "
        "WHERE capturecard.hostname = :HOSTNAME ";

    if (!rawtype.isEmpty())
        qstr += " AND capturecard.cardtype = :INPUTTYPE";

    query.prepare(qstr);

    if (!rawtype.isEmpty())
        query.bindValue(":INPUTTYPE", rawtype.toUpper());

    query.bindValue(":HOSTNAME", hostname);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::IsInputTypePresent", query);
        return false;
    }

    uint count = 0;
    if (query.next())
        count = query.value(0).toUInt();

    return count > 0;
}

CardUtil::InputTypes CardUtil::GetInputTypes(void)
{
    InputTypes inputtypes;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT cardtype, videodevice "
                  "FROM capturecard");

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputTypes()", query);
    }
    else
    {
        QString cardtype;

        while (query.next())
        {
            cardtype = query.value(0).toString();
            if (cardtype != "V4L2ENC")
                inputtypes[cardtype] = "";
#ifdef USING_V4L2
            else
            {
                V4L2util v4l2(query.value(1).toString());
                if (v4l2.IsOpen())
                {
                    QString driver_name = "V4L2:" + v4l2.DriverName();
                    inputtypes[driver_name] = v4l2.CardName();
                }
            }
#endif
        }
    }

    return inputtypes;
}

/**
 * Get a list of card input types for a source id.
 *
 * \param sourceid [in] source id.
 * \return QStringList of card types for that source
*/

QStringList CardUtil::GetInputTypeNames(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard "
                  "WHERE capturecard.sourceid = :SOURCEID "
                  "GROUP BY cardtype");
    query.bindValue(":SOURCEID", sourceid);

    QStringList list;
    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputTypes", query);
        return list;
    }
    while (query.next())
        list.push_back(query.value(0).toString());
    return list;
}




/** \fn CardUtil::GetVideoDevices(const QString&, QString)
 *  \brief Returns the videodevices of the matching inputs, duplicates removed
 *  \param rawtype  Input type as used in DB or empty string for all inputids
 *  \param hostname Host to check, or empty string for current host
 */
QStringList CardUtil::GetVideoDevices(const QString &rawtype, QString hostname)
{
    QStringList list;

    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT videodevice "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME";

    if (!rawtype.isEmpty())
        qstr += " AND cardtype = :INPUTTYPE";

    query.prepare(qstr);

    if (!rawtype.isEmpty())
        query.bindValue(":INPUTTYPE", rawtype.toUpper());

    query.bindValue(":HOSTNAME", hostname);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetVideoDevices", query);
        return list;
    }

    QMap<QString,bool> dup;
    while (query.next())
    {
        QString videodevice = query.value(0).toString();
        if (dup[videodevice])
            continue;

        list.push_back(videodevice);
        dup[videodevice] = true;
    }

    return list;
}

QMap <QString,QStringList> CardUtil::videoDeviceCache;

void CardUtil::ClearVideoDeviceCache()
{
    videoDeviceCache.clear();
}


QStringList CardUtil::ProbeVideoDevices(const QString &rawtype)
{
    if (videoDeviceCache.contains(rawtype))
        return videoDeviceCache[rawtype];

    QStringList devs;

    if (rawtype.toUpper() == "DVB")
    {
        QDir dir("/dev/dvb", "adapter*", QDir::Name, QDir::Dirs);
        const QFileInfoList il = dir.entryInfoList();

        QFileInfoList::const_iterator it = il.begin();

        for (; it != il.end(); ++it)
        {
            QDir subdir(it->filePath(), "frontend*", QDir::Name, QDir::Files | QDir::System);
            const QFileInfoList subil = subdir.entryInfoList();
            if (subil.isEmpty())
                continue;

            QFileInfoList::const_iterator subit = subil.begin();
            for (; subit != subil.end(); ++subit)
                devs.push_back(subit->filePath());
        }
    }
    else if (rawtype.toUpper() == "ASI")
    {
        QDir dir("/dev/", "asirx*", QDir::Name, QDir::System);
        const QFileInfoList il = dir.entryInfoList();

        QFileInfoList::const_iterator it = il.begin();
        for (; it != il.end(); ++it)
        {
            if (GetASIDeviceNumber(it->filePath()) >= 0)
            {
                devs.push_back(it->filePath());
                continue;
            }
            break;
        }
    }
#ifdef USING_HDHOMERUN
    else if (rawtype.toUpper() == "HDHOMERUN")
    {
        uint32_t  target_ip   = 0;
        uint32_t  device_type = HDHOMERUN_DEVICE_TYPE_TUNER;
        uint32_t  device_id   = HDHOMERUN_DEVICE_ID_WILDCARD;
        const int max_count   = 50;
        hdhomerun_discover_device_t result_list[max_count];

        int result = hdhomerun_discover_find_devices_custom(
            target_ip, device_type, device_id, result_list, max_count);

        if (result == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error finding HDHomerun devices");
        }

        if (result >= max_count)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                "Warning: may be > 50 HDHomerun devices");
        }

        // Return "deviceid ipaddress" pairs
        for (int i = 0; i < result; i++)
        {
            QString id = QString("%1").arg(result_list[i].device_id, 0, 16);
            QString ip = QString("%1.%2.%3.%4")
                                 .arg((result_list[i].ip_addr>>24) & 0xFF)
                                 .arg((result_list[i].ip_addr>>16) & 0xFF)
                                 .arg((result_list[i].ip_addr>> 8) & 0xFF)
                                 .arg((result_list[i].ip_addr>> 0) & 0xFF);

            for (int tuner = 0; tuner < result_list[i].tuner_count; tuner++)
            {
                QString hdhrdev = id.toUpper() + " " + ip + " " +
                                  QString("%1").arg(tuner);
                devs.push_back(hdhrdev);
            }
        }
    }
#endif // USING_HDHOMERUN
#ifdef USING_VBOX
    else if (rawtype.toUpper() == "VBOX")
    {
        devs = VBox::probeDevices();
    }
#endif // USING_VBOX
#ifdef USING_CETON
    else if (rawtype.toUpper() == "CETON")
    {
        // TODO implement CETON probing.
        LOG(VB_GENERAL, LOG_INFO, "CardUtil::ProbeVideoDevices: "
            "TODO Probe Ceton devices");
    }
#endif // USING_CETON
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Raw Type: '%1' is not supported")
                                     .arg(rawtype));
    }

    videoDeviceCache.insert(rawtype,devs);
    return devs;
}

QString CardUtil::ProbeDVBType(const QString &device)
{
    QString ret = "ERROR_UNKNOWN";

    if (device.isEmpty())
        return ret;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toLatin1();

    int fd_frontend = open(dev.constData(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Can't open DVB frontend (%1) for %2." + ENO)
                .arg(dvbdev).arg(device));
        return ret;
    }

    struct dvb_frontend_info info;
    memset(&info, 0, sizeof(info));
    int err = ioctl(fd_frontend, FE_GET_INFO, &info);
    if (err < 0)
    {
        close(fd_frontend);
        LOG(VB_GENERAL, LOG_ERR, QString("FE_GET_INFO ioctl failed (%1)")
                                         .arg(dvbdev) + ENO);
        return ret;
    }

    DTVTunerType type(info.type);
#if HAVE_FE_CAN_2G_MODULATION
    if (info.caps & FE_CAN_2G_MODULATION)
    {
        if (type == DTVTunerType::kTunerTypeDVBS1)
            type = DTVTunerType::kTunerTypeDVBS2;
        else if (type == DTVTunerType::kTunerTypeDVBT)
            type = DTVTunerType::kTunerTypeDVBT2;
    }
#endif // HAVE_FE_CAN_2G_MODULATION

#if DVB_API_VERSION >=5
    unsigned int i;
    struct dtv_property prop;
    struct dtv_properties cmd;

    memset(&prop, 0, sizeof(prop));
    prop.cmd = DTV_ENUM_DELSYS;
    cmd.num = 1;
    cmd.props = &prop;

    if (ioctl(fd_frontend, FE_GET_PROPERTY, &cmd) == 0)
    {
        for (i = 0; i < prop.u.buffer.len; i++)
        {
            switch (prop.u.buffer.data[i])
            {
                // TODO: not supported. you can have DVBC and DVBT on the same card
                // The following are backwards compatible so its ok
                case SYS_DVBS2:
                    type = DTVTunerType::kTunerTypeDVBS2;
                    break;
                case SYS_DVBT2:
                    type = DTVTunerType::kTunerTypeDVBT2;
                    break;
                default:
                    break;
            }
        }
    }
#endif
    close(fd_frontend);

    ret = (type.toString() != "UNKNOWN") ? type.toString().toUpper() : ret;
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::ProbeDVBFrontendName(const QString &)
 *  \brief Returns the input type from the video device
 */
QString CardUtil::ProbeDVBFrontendName(const QString &device)
{
    QString ret = "ERROR_UNKNOWN";
    (void) device;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toLatin1();
    int fd_frontend = open(dev.constData(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
        return "ERROR_OPEN";

    struct dvb_frontend_info info;
    memset(&info, 0, sizeof(info));
    int err = ioctl(fd_frontend, FE_GET_INFO, &info);
    if (err < 0)
    {
        close(fd_frontend);
        return "ERROR_PROBE";
    }

    ret = info.name;

    close(fd_frontend);
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::HasDVBCRCBug(const QString &)
 *  \brief Returns true if and only if the device munges
 *         PAT/PMT tables, and then doesn't fix the CRC.
 *
 *   Currently the list of broken DVB hardware and drivers includes:
 *   "VLSI VES1x93 DVB-S", and "ST STV0299 DVB-S"
 *
 *  Note: "DST DVB-S" was on this list but has been verified to not
 *        mess up the PAT using Linux 2.6.18.1.
 *
 *  Note: "Philips TDA10046H DVB-T" was on this list but has been
 *        verified to not mess up the PMT with a recent kernel and
 *        firmware (See https://code.mythtv.org/trac/ticket/3541).
 *
 *  \param device Open DVB frontend device file descriptor to be checked
 *  \return true iff the device munges tables, so that they fail a CRC check.
 */
bool CardUtil::HasDVBCRCBug(const QString &device)
{
    QString name = ProbeDVBFrontendName(device);
    return ((name == "VLSI VES1x93 DVB-S")      || // munges PMT
            (name == "ST STV0299 DVB-S"));         // munges PAT
}

uint CardUtil::GetMinSignalMonitoringDelay(const QString &device)
{
    QString name = ProbeDVBFrontendName(device);
    if (name.indexOf("DVB-S") >= 0)
        return 300;
    if (name == "DiBcom 3000P/M-C DVB-T")
        return 100;
    return 25;
}

QString CardUtil::ProbeSubTypeName(uint inputid)
{
    QString type = GetRawInputType(inputid);
    if ("DVB" != type)
        return type;

    QString device = GetVideoDevice(inputid);

    if (device.isEmpty())
        return "ERROR_OPEN";

    return ProbeDVBType(device);
}

/// \brief Returns true iff the input_type is one of the DVB types.
bool CardUtil::IsDVBInputType(const QString &inputType)
{
    QString t = inputType.toUpper();
    return (t == "DVB") || (t == "QAM") || (t == "QPSK") ||
        (t == "OFDM") || (t == "ATSC") || (t == "DVB_S2");
}

QString get_on_input(const QString &to_get, uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("SELECT %1 ").arg(to_get) +
        "FROM capturecard "
        "WHERE capturecard.cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::get_on_source", query);
    else if (query.next())
        return query.value(0).toString();

    return QString();
}

bool set_on_input(const QString &to_set, uint inputid, const QString &value)
{
    QString tmp = get_on_input("capturecard.cardid", inputid);
    if (tmp.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("UPDATE capturecard SET %1 = :VALUE ").arg(to_set) +
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    query.bindValue(":VALUE",  value);

    if (query.exec())
        return true;

    MythDB::DBError("CardUtil::set_on_input", query);
    return false;
}

/**
 *  \brief Returns all inputids of inputs that uses the specified
 *         videodevice if specified, and optionally rawtype and a non-local
 *         hostname. The result is ordered from smallest to largest.
 *  \param videodevice Video device we want input ids for
 *  \param rawtype     Input type as used in DB or empty string for any type
 *  \param inputname   The name of the input card.
 *  \param hostname    Host on which device resides, only
 *                     required if said host is not the localhost
 */
vector<uint> CardUtil::GetInputIDs(QString videodevice,
                                   QString rawtype,
                                   QString inputname,
                                   QString hostname)
{
    vector<uint> list;

    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardid "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME ";
    if (!videodevice.isEmpty())
        qstr += "AND videodevice = :DEVICE ";
    if (!inputname.isEmpty())
        qstr += "AND inputname = :INPUTNAME ";
    if (!rawtype.isEmpty())
        qstr += "AND cardtype = :INPUTTYPE ";
    qstr += "ORDER BY cardid";

    query.prepare(qstr);

    query.bindValue(":HOSTNAME", hostname);
    if (!videodevice.isEmpty())
        query.bindValue(":DEVICE", videodevice);
    if (!inputname.isEmpty())
        query.bindValue(":INPUTNAME", inputname);
    if (!rawtype.isEmpty())
        query.bindValue(":INPUTTYPE", rawtype.toUpper());

    if (!query.exec())
        MythDB::DBError("CardUtil::GetInputIDs(videodevice...)", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

uint CardUtil::GetChildInputCount(uint inputid)
{
    if (!inputid)
        return 0;

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT COUNT(*) "
        "FROM capturecard "
        "WHERE parentid = :INPUTID";

    query.prepare(qstr);
    query.bindValue(":INPUTID", inputid);

    uint count = 0;

    if (!query.exec())
        MythDB::DBError("CardUtil::GetChildInputCount()", query);
    else if (query.next())
        count = query.value(0).toUInt();

    return count;
}

vector<uint> CardUtil::GetChildInputIDs(uint inputid)
{
    vector<uint> list;

    if (!inputid)
        return list;

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT cardid "
        "FROM capturecard "
        "WHERE parentid = :INPUTID "
        "ORDER BY cardid";

    query.prepare(qstr);
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetChildInputIDs()", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

static uint clone_capturecard(uint src_inputid, uint orig_dst_inputid)
{
    uint dst_inputid = orig_dst_inputid;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!dst_inputid)
    {
        query.prepare(
            "DELETE FROM capturecard "
            "WHERE videodevice = 'temp_dummy'");

        if (!query.exec())
        {
            MythDB::DBError("clone_capturecard -- delete temp", query);
            return 0;
        }

        query.prepare(
            "INSERT INTO capturecard "
            "SET videodevice = 'temp_dummy'");

        if (!query.exec())
        {
            MythDB::DBError("clone_capturecard -- insert temp", query);
            return 0;
        }

        query.prepare(
            "SELECT cardid "
            "FROM capturecard "
            "WHERE videodevice = 'temp_dummy'");

        if (!query.exec())
        {
            MythDB::DBError("clone_capturecard -- get temp id", query);
            return 0;
        }

        if (!query.next())
        {
            LOG(VB_GENERAL, LOG_ERR, "clone_capturecard -- get temp id");
            return 0;
        }

        dst_inputid = query.value(0).toUInt();
    }

    query.prepare(
        "SELECT videodevice,           audiodevice,           vbidevice,      "
        "       cardtype,              hostname,              signal_timeout, "
        "       channel_timeout,       dvb_wait_for_seqstart, dvb_on_demand,  "
        "       dvb_tuning_delay,      dvb_diseqc_type,       diseqcid,       "
        "       dvb_eitscan,           inputname,             sourceid,       "
        "       externalcommand,       changer_device,        changer_model,  "
        "       tunechan,              startchan,             displayname,    "
        "       dishnet_eit,           recpriority,           quicktune,      "
        "       livetvorder,           reclimit,                              "
        // See below for special handling of the following.
        "       schedgroup,            schedorder                             "
        "FROM capturecard "
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", src_inputid);

    if (!query.exec())
    {
        MythDB::DBError("clone_capturecard -- get data", query);
        return 0;
    }
    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, "clone_cardinput -- get data 2");
        return 0;
    }

    // Hangel schedgroup and schedorder specially.  If schedgroup is
    // set, schedgroup and schedorder should be false and 0,
    // respectively, for all children.
    bool schedgroup = query.value(26).toBool();
    uint schedorder = query.value(27).toUInt();
    if (schedgroup)
    {
        schedgroup = false;
        schedorder = 0;
    }

    MSqlQuery query2(MSqlQuery::InitCon());
    query2.prepare(
        "UPDATE capturecard "
        "SET videodevice           = :V0, "
        "    audiodevice           = :V1, "
        "    vbidevice             = :V2, "
        "    cardtype              = :V3, "
        "    hostname              = :V4, "
        "    signal_timeout        = :V5, "
        "    channel_timeout       = :V6, "
        "    dvb_wait_for_seqstart = :V7, "
        "    dvb_on_demand         = :V8, "
        "    dvb_tuning_delay      = :V9, "
        "    dvb_diseqc_type       = :V10, "
        "    diseqcid              = :V11,"
        "    dvb_eitscan           = :V12, "
        "    inputname             = :V13, "
        "    sourceid              = :V14, "
        "    externalcommand       = :V15, "
        "    changer_device        = :V16, "
        "    changer_model         = :V17, "
        "    tunechan              = :V18, "
        "    startchan             = :V19, "
        "    displayname           = :V20, "
        "    dishnet_eit           = :V21, "
        "    recpriority           = :V22, "
        "    quicktune             = :V23, "
        "    livetvorder           = :V24, "
        "    reclimit              = :V25, "
        "    schedgroup            = :SCHEDGROUP, "
        "    schedorder            = :SCHEDORDER, "
        "    parentid              = :PARENTID "
        "WHERE cardid = :INPUTID");
    for (uint i = 0; i < 26; ++i)
        query2.bindValue(QString(":V%1").arg(i), query.value(i).toString());
    query2.bindValue(":INPUTID", dst_inputid);
    query2.bindValue(":PARENTID", src_inputid);
    query2.bindValue(":SCHEDGROUP", schedgroup);
    query2.bindValue(":SCHEDORDER", schedorder);

    if (!query2.exec())
    {
        MythDB::DBError("clone_capturecard -- save data", query2);
        if (!orig_dst_inputid)
            CardUtil::DeleteInput(dst_inputid);
        return 0;
    }

    // copy input group linkages
    vector<uint> src_grps = CardUtil::GetInputGroups(src_inputid);
    vector<uint> dst_grps = CardUtil::GetInputGroups(dst_inputid);
    for (uint j = 0; j < dst_grps.size(); j++)
        CardUtil::UnlinkInputGroup(dst_inputid, dst_grps[j]);
    for (uint j = 0; j < src_grps.size(); j++)
        CardUtil::LinkInputGroup(dst_inputid, src_grps[j]);

    // clone diseqc_config (just points to the same diseqc_tree row)
    DiSEqCDevSettings diseqc;
    if (diseqc.Load(src_inputid))
        diseqc.Store(dst_inputid);

    return dst_inputid;
}

uint CardUtil::CloneCard(uint src_inputid, uint orig_dst_inputid)
{
    QString type = CardUtil::GetRawInputType(src_inputid);
    if (!IsTunerSharingCapable(type))
        return 0;

    uint dst_inputid = clone_capturecard(src_inputid, orig_dst_inputid);
    return dst_inputid;
}

uint CardUtil::AddChildInput(uint parentid)
{
    uint inputid = CloneCard(parentid, 0);

    // Update the reclimit for the parent and all children so the new
    // child doesn't get removed the next time mythtv-setup is run.
    if (inputid)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Added child input %1 to parent %2")
            .arg(inputid).arg(parentid));
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("UPDATE capturecard "
                      "SET reclimit = reclimit + 1 "
                      "WHERE cardid = :PARENTID");
        query.bindValue(":PARENTID", parentid);
        if (!query.exec())
            MythDB::DBError("CardUtil::AddChildInput", query);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to add child input to parent %1").arg(parentid));
    }

    return inputid;
}

QString CardUtil::GetFirewireChangerNode(uint inputid)
{
    QString fwnode;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT changer_device "
                  "FROM capturecard WHERE cardid = :INPUTID ");
    query.bindValue(":INPUTID", inputid);

    if (query.exec() && query.next())
    {
        fwnode = query.value(0).toString();
    }

    return fwnode;
}

QString CardUtil::GetFirewireChangerModel(uint inputid)
{
    QString fwnode;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT changer_model "
                  "FROM capturecard WHERE cardid = :INPUTID ");
    query.bindValue(":INPUTID", inputid);

    if (query.exec() && query.next())
    {
        fwnode = query.value(0).toString();
    }

    return fwnode;
}

vector<uint> CardUtil::GetInputIDs(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM capturecard "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    vector<uint> list;

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

bool CardUtil::SetStartChannel(uint inputid, const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE capturecard "
                  "SET startchan = :CHANNUM "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
    {
        MythDB::DBError("set_startchan", query);
        return false;
    }

    return true;
}

bool CardUtil::GetInputInfo(InputInfo &input, vector<uint> *groupids)
{
    if (!input.inputid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                  "inputname, sourceid, livetvorder, "
                  "schedorder, displayname, recpriority, quicktune "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", input.inputid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputInfo()", query);
        return false;
    }

    if (!query.next())
        return false;

    input.name     = query.value(0).toString();
    input.sourceid = query.value(1).toUInt();
    input.livetvorder = query.value(2).toUInt();
    input.scheduleOrder = query.value(3).toUInt();
    input.displayName = query.value(4).toString();
    input.recPriority = query.value(5).toInt();
    input.quickTune = query.value(6).toBool();

    if (input.displayName.isEmpty())
        input.displayName = QObject::tr("Input %1:%2")
            .arg(input.inputid).arg(input.name);

    if (groupids)
        *groupids = GetInputGroups(input.inputid);

    return true;
}

QList<InputInfo> CardUtil::GetAllInputInfo()
{
    QList<InputInfo> infoInputList;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardid, "
                  "inputname, sourceid, livetvorder, "
                  "schedorder, displayname, recpriority, quicktune "
                  "FROM capturecard");

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetAllInputInfo()", query);
        return infoInputList;
    }

    while (query.next())
    {
        InputInfo input;
        input.inputid  = query.value(0).toUInt();
        input.name     = query.value(1).toString();
        input.sourceid = query.value(2).toUInt();
        input.livetvorder = query.value(3).toUInt();
        input.scheduleOrder = query.value(4).toUInt();
        input.displayName = query.value(5).toString();
        input.recPriority = query.value(6).toInt();
        input.quickTune = query.value(7).toBool();

        infoInputList.push_back(input);
    }

    return infoInputList;
}

QString CardUtil::GetInputName(uint inputid)
{
    InputInfo info("None", 0, inputid, 0, 0, 0);
    GetInputInfo(info);
    return info.name;
}

QString CardUtil::GetStartingChannel(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT startchan "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetStartingChannel(uint)", query);
    else if (query.next())
        return query.value(0).toString();

    return QString();
}

QString CardUtil::GetDisplayName(uint inputid)
{
    if (!inputid)
        return QString();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT displayname, cardid, cardtype, inputname "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetDisplayName(uint)", query);
    else if (query.next())
    {
        QString result = query.value(0).toString();
        if (result.isEmpty())
            result = QString("%1: %2/%3").arg(query.value(1).toInt())
                .arg(query.value(2).toString()).arg(query.value(3).toString());
        return result;
    }

    return QString();
}

uint CardUtil::GetSourceID(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid "
        "FROM capturecard "
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetSourceID()", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

// Is this intentionally leaving out the hostname when updating the
// capturecard table? The hostname value does get set when inserting
// into the capturecard table. (Code written in 2011.)
int CardUtil::CreateCardInput(const uint inputid,
                              const uint sourceid,
                              const QString &inputname,
                              const QString &externalcommand,
                              const QString &changer_device,
                              const QString &changer_model,
                              const QString &/*hostname*/,
                              const QString &tunechan,
                              const QString &startchan,
                              const QString &displayname,
                              bool          dishnet_eit,
                              const uint recpriority,
                              const uint quicktune,
                              const uint schedorder,
                              const uint livetvorder)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "UPDATE capturecard "
        "SET sourceid = :SOURCEID, "
        "    inputname = :INPUTNAME, "
        "    externalcommand = :EXTERNALCOMMAND, "
        "    changer_device = :CHANGERDEVICE, "
        "    changer_model = :CHANGERMODEL, "
        "    tunechan = :TUNECHAN, "
        "    startchan = :STARTCHAN, "
        "    displayname = :DISPLAYNAME, "
        "    dishnet_eit = :DISHNETEIT, "
        "    recpriority = :RECPRIORITY, "
        "    quicktune = :QUICKTUNE, "
        "    schedorder = :SCHEDORDER, "
        "    livetvorder = :LIVETVORDER "
        "WHERE cardid = :INPUTID AND "
        "      inputname = 'None'");

    query.bindValue(":INPUTID", inputid);
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":INPUTNAME", inputname);
    query.bindValue(":EXTERNALCOMMAND", externalcommand);
    query.bindValue(":CHANGERDEVICE", changer_device);
    query.bindValue(":CHANGERMODEL", changer_model);
    query.bindValue(":TUNECHAN", tunechan);
    query.bindValue(":STARTCHAN", startchan);
    query.bindValue(":DISPLAYNAME", displayname.isNull() ? "" : displayname);
    query.bindValue(":DISHNETEIT", dishnet_eit);
    query.bindValue(":RECPRIORITY", recpriority);
    query.bindValue(":QUICKTUNE", quicktune);
    query.bindValue(":SCHEDORDER", schedorder);
    query.bindValue(":LIVETVORDER", livetvorder);

    if (!query.exec())
    {
        MythDB::DBError("CreateCardInput", query);
        return -1;
    }

    return inputid;
}

uint CardUtil::CreateInputGroup(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT inputgroupid FROM inputgroup "
                  "WHERE inputgroupname = :GROUPNAME "
                  "LIMIT 1");
    query.bindValue(":GROUPNAME", name);
    if (!query.exec())
    {
        MythDB::DBError("CreateNewInputGroup 0", query);
        return 0;
    }

    if (query.next())
        return query.value(0).toUInt();

    query.prepare("SELECT MAX(inputgroupid) FROM inputgroup");
    if (!query.exec())
    {
        MythDB::DBError("CreateNewInputGroup 1", query);
        return 0;
    }

    uint inputgroupid = (query.next()) ? query.value(0).toUInt() + 1 : 1;

    query.prepare(
        "INSERT INTO inputgroup "
        "       (cardinputid, inputgroupid, inputgroupname) "
        "VALUES (:INPUTID,    :GROUPID,     :GROUPNAME    ) ");
    query.bindValue(":INPUTID",   0);
    query.bindValue(":GROUPID",   inputgroupid);
    query.bindValue(":GROUPNAME", name);
    if (!query.exec())
    {
        MythDB::DBError("CreateNewInputGroup 2", query);
        return 0;
    }

    return inputgroupid;
}

uint CardUtil::CreateDeviceInputGroup(uint inputid,
                                      const QString &type,
                                      const QString &host,
                                      const QString &device)
{
    QString name = host + '|' + device;
    if (type == "FREEBOX" || type == "IMPORT" ||
        type == "DEMO"    || type == "EXTERNAL")
        name += QString("|%1").arg(inputid);
    return CreateInputGroup(name);
}

uint CardUtil::GetDeviceInputGroup(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT inputgroupid "
        "FROM inputgroup "
        "WHERE cardinputid = :INPUTID "
        "      AND inputgroupname REGEXP '^[a-z_-]*\\\\|'");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetDeviceInputGroup()", query);
        return false;
    }

    if (query.next())
    {
        return query.value(0).toUInt();
    }

    return 0;
}

bool CardUtil::LinkInputGroup(uint inputid, uint inputgroupid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT cardinputid, inputgroupid, inputgroupname "
        "FROM inputgroup "
        "WHERE inputgroupid = :GROUPID "
        "ORDER BY inputgroupid, cardinputid, inputgroupname");
    query.bindValue(":GROUPID", inputgroupid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::CreateInputGroup() 1", query);
        return false;
    }

    if (!query.next())
        return false;

    const QString name = query.value(2).toString();

    query.prepare(
        "INSERT INTO inputgroup "
        "       (cardinputid, inputgroupid, inputgroupname) "
        "VALUES (:INPUTID,    :GROUPID,     :GROUPNAME    ) ");

    query.bindValue(":INPUTID",   inputid);
    query.bindValue(":GROUPID",   inputgroupid);
    query.bindValue(":GROUPNAME", name);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::CreateInputGroup() 2", query);
        return false;
    }

    return true;
}

bool CardUtil::UnlinkInputGroup(uint inputid, uint inputgroupid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!inputid && !inputgroupid)
    {
        query.prepare(
            "DELETE FROM inputgroup "
            "WHERE cardinputid NOT IN "
            "( SELECT cardid FROM capturecard )");
    }
    else
    {
        query.prepare(
            "DELETE FROM inputgroup "
            "WHERE cardinputid  = :INPUTID AND "
            "      inputgroupid = :GROUPID ");

        query.bindValue(":INPUTID", inputid);
        query.bindValue(":GROUPID", inputgroupid);
    }

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::DeleteInputGroup()", query);
        return false;
    }

    return true;
}

vector<uint> CardUtil::GetInputGroups(uint inputid)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT inputgroupid "
        "FROM inputgroup "
        "WHERE cardinputid = :INPUTID "
        "ORDER BY inputgroupid, cardinputid, inputgroupname");

    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputGroups()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

vector<uint> CardUtil::GetGroupInputIDs(uint inputgroupid)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM capturecard, inputgroup "
        "WHERE inputgroupid = :GROUPID AND "
        "      capturecard.cardid = inputgroup.cardinputid "
        "ORDER BY cardid");

    query.bindValue(":GROUPID", inputgroupid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetGroupInputIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

vector<uint> CardUtil::GetConflictingInputs(uint inputid)
{
    LOG(VB_RECORD, LOG_INFO,
        LOC + QString("GetConflictingInputs() input %1").arg(inputid));

    vector<uint> inputids;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT c.cardid "
        "FROM ( "
        "    SELECT inputgroupid "
        "    FROM inputgroup "
        "    WHERE cardinputid = :INPUTID1 "
        ") g "
        "JOIN inputgroup ig ON ig.inputgroupid = g.inputgroupid "
        "JOIN capturecard c ON c.cardid = ig.cardinputid "
        "                      AND c.cardid <> :INPUTID2 "
        "ORDER BY c.cardid");

    query.bindValue(":INPUTID1", inputid);
    query.bindValue(":INPUTID2", inputid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetConflictingInputs()", query);
        return inputids;
    }

    while (query.next())
    {
        inputids.push_back(query.value(0).toUInt());
        LOG(VB_RECORD, LOG_INFO,
            LOC + QString("GetConflictingInputs() got input %1").arg(inputids.back()));
    }

    return inputids;
}

bool CardUtil::GetTimeouts(uint inputid,
                           uint &signal_timeout, uint &channel_timeout)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT signal_timeout, channel_timeout "
        "FROM capturecard "
        "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetTimeouts()", query);
    else if (query.next())
    {
        signal_timeout  = (uint) max(query.value(0).toInt(), 250);
        channel_timeout = (uint) max(query.value(1).toInt(), 500);
        return true;
    }

    return false;
}

bool CardUtil::IsInNeedOfExternalInputConf(uint inputid)
{
    DiSEqCDev dev;
    DiSEqCDevTree *diseqc_tree = dev.FindTree(inputid);

    bool needsConf = false;
    if (diseqc_tree)
        needsConf = diseqc_tree->IsInNeedOfConf();

    return needsConf;
}

uint CardUtil::GetQuickTuning(uint inputid, const QString &input_name)
{
    uint quicktune = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT quicktune "
        "FROM capturecard "
        "WHERE cardid    = :INPUTID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":INPUTID",    inputid);
    query.bindValue(":INPUTNAME", input_name);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetQuickTuning()", query);
    else if (query.next())
        quicktune = query.value(0).toUInt();

    return quicktune;
}

bool CardUtil::hasV4L2(int videofd)
{
    (void) videofd;
#ifdef USING_V4L2
    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));

    return ((ioctl(videofd, VIDIOC_QUERYCAP, &vcap) >= 0) &&
            (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE));
#else // if !USING_V4L2
    return false;
#endif // !USING_V4L2
}

bool CardUtil::GetV4LInfo(
    int videofd, QString &input, QString &driver, uint32_t &version,
    uint32_t &capabilities)
{
    input.clear();
    driver.clear();
    version = 0;
    capabilities = 0;

    if (videofd < 0)
        return false;

#ifdef USING_V4L2
    // First try V4L2 query
    struct v4l2_capability capability;
    memset(&capability, 0, sizeof(struct v4l2_capability));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &capability) >= 0)
    {
        input = QString::fromLatin1((const char*)capability.card);
        driver = QString::fromLatin1((const char*)capability.driver);
        version = capability.version;
        capabilities = capability.capabilities;
    }
#ifdef USING_V4L1
    else // Fallback to V4L1 query
    {
        struct video_capability capability;
        if (ioctl(videofd, VIDIOCGCAP, &capability) >= 0)
            input = QString::fromLatin1((const char*)capability.name);
    }
#endif // USING_V4L1
#endif // USING_V4L2

    if (!driver.isEmpty())
        driver.remove( QRegExp("\\[[0-9]\\]$") );

    return !input.isEmpty();
}

InputNames CardUtil::ProbeV4LVideoInputs(int videofd, bool &ok)
{
    (void) videofd;

    InputNames list;
    ok = false;

#ifdef USING_V4L2
    bool usingv4l2 = hasV4L2(videofd);

    // V4L v2 query
    struct v4l2_input vin;
    memset(&vin, 0, sizeof(vin));
    while (usingv4l2 && (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0))
    {
        QString input((char *)vin.name);
        list[vin.index] = input;
        vin.index++;
    }
    if (vin.index)
    {
        ok = true;
        return list;
    }

#ifdef USING_V4L1
    // V4L v1 query
    struct video_capability vidcap;
    memset(&vidcap, 0, sizeof(vidcap));
    if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0)
    {
        QString msg = QObject::tr("Could not query inputs.");
        LOG(VB_GENERAL, LOG_ERR, "ProbeV4LVideoInputs(): Error, " + msg + ENO);
        list[-1] = msg;
        vidcap.channels = 0;
    }

    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        memset(&test, 0, sizeof(test));
        test.channel = i;

        if (ioctl(videofd, VIDIOCGCHAN, &test) != 0)
        {
            LOG(VB_GENERAL, LOG_ERR, "ProbeV4LVideoInputs(): Error, " +
                    QString("Could determine name of input #%1"
                            "\n\t\t\tNot adding it to the list.")
                    .arg(test.channel) + ENO);
            continue;
        }

        list[i] = test.name;
    }
#endif // USING_V4L1

    // Create an input when none are advertised
    if (list.isEmpty())
        list[0] = "Television";

    ok = true;
#else // if !USING_V4L2
    list[-1] += QObject::tr("ERROR, Compile with V4L support to query inputs");
#endif // !USING_V4L2
    return list;
}

InputNames CardUtil::ProbeV4LAudioInputs(int videofd, bool &ok)
{
    (void) videofd;

    InputNames list;
    ok = false;

#ifdef USING_V4L2
    bool usingv4l2 = hasV4L2(videofd);

    // V4L v2 query
    struct v4l2_audio ain;
    memset(&ain, 0, sizeof(ain));
    while (usingv4l2 && (ioctl(videofd, VIDIOC_ENUMAUDIO, &ain) >= 0))
    {
        QString input((char *)ain.name);
        list[ain.index] = input;
        ain.index++;
    }
    if (ain.index)
    {
        ok = true;
        return list;
    }

    ok = true;
#else // if !USING_V4L2
    list[-1] += QObject::tr(
        "ERROR, Compile with V4L support to query audio inputs");
#endif // !USING_V4L2
    return list;
}

InputNames CardUtil::GetConfiguredDVBInputs(const QString &device)
{
    InputNames list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid, inputname "
        "FROM capturecard "
        "WHERE hostname = :HOSTNAME "
        "      AND videodevice = :DEVICE "
        "      AND parentid = 0 "
        "      AND inputname <> 'None'");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    query.bindValue(":DEVICE", device);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetConfiguredDVBInputs", query);
    else
    {
        while (query.next())
            list[query.value(0).toUInt()] = query.value(1).toString();
    }
    return list;
}

QStringList CardUtil::ProbeVideoInputs(QString device, QString inputtype)
{
    QStringList ret;

    if (IsSingleInputType(inputtype))
        ret += "MPEG2TS";
    else if ("DVB" == inputtype)
        ret += ProbeDVBInputs(device);
    else
        ret += ProbeV4LVideoInputs(device);

    return ret;
}

QStringList CardUtil::ProbeAudioInputs(QString device, QString inputtype)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("ProbeAudioInputs(%1,%2)")
                                   .arg(device).arg(inputtype));
    QStringList ret;

    if ("HDPVR" == inputtype ||
        "V4L2"  == inputtype)
        ret += ProbeV4LAudioInputs(device);

    return ret;
}

QStringList CardUtil::ProbeV4LVideoInputs(QString device)
{
    bool ok;
    QStringList ret;
    QByteArray dev = device.toLatin1();
    int videofd = open(dev.constData(), O_RDWR);
    if (videofd < 0)
    {
        ret += QObject::tr("Could not open '%1' "
                           "to probe its inputs.").arg(device);
        return ret;
    }
    InputNames list = CardUtil::ProbeV4LVideoInputs(videofd, ok);
    close(videofd);

    if (!ok)
    {
        ret += list[-1];
        return ret;
    }

    InputNames::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if (it.key() >= 0)
            ret += *it;
    }

    return ret;
}

QStringList CardUtil::ProbeV4LAudioInputs(QString device)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("ProbeV4LAudioInputs(%1)").arg(device));

    bool ok;
    QStringList ret;
    int videofd = open(device.toLatin1().constData(), O_RDWR);
    if (videofd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "ProbeAudioInputs() -> couldn't open device");
        ret += QObject::tr("Could not open '%1' to probe its inputs.")
                   .arg(device);
        return ret;
    }
    InputNames list = CardUtil::ProbeV4LAudioInputs(videofd, ok);
    close(videofd);

    if (!ok)
    {
        ret += list[-1];
        return ret;
    }

    InputNames::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if (it.key() >= 0)
            ret += *it;
    }

    return ret;
}

QStringList CardUtil::ProbeDVBInputs(QString device)
{
    QStringList ret;

#ifdef USING_DVB
    InputNames list = GetConfiguredDVBInputs(device);
    InputNames::iterator it;
    for (it = list.begin(); it != list.end(); ++it)
    {
        if (it.key())
            ret += *it;
    }
#else
    (void) device;
    ret += QObject::tr("ERROR, Compile with DVB support to query inputs");
#endif

    return ret;
}

QString CardUtil::GetDeviceLabel(const QString &inputtype,
                                 const QString &videodevice)
{
    return QString("[ %1 : %2 ]").arg(inputtype).arg(videodevice);
}

QString CardUtil::GetDeviceLabel(uint inputid)
{
    QString devlabel;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype, videodevice "
                  "FROM capturecard WHERE cardid = :INPUTID ");
    query.bindValue(":INPUTID", inputid);

    if (query.exec() && query.next())
    {
        return GetDeviceLabel(query.value(0).toString(),
                              query.value(1).toString());
    }

    return "[ UNKNOWN ]";
}

void CardUtil::GetDeviceInputNames(
    const QString      &device,
    const QString      &inputtype,
    QStringList        &inputs)
{
    inputs.clear();
    if (IsSingleInputType(inputtype))
        inputs += "MPEG2TS";
    else if (inputtype == "DVB")
        inputs += "DVBInput";
    else
        inputs += ProbeV4LVideoInputs(device);
}

int CardUtil::CreateCaptureCard(const QString &videodevice,
                                 const QString &audiodevice,
                                 const QString &vbidevice,
                                 const QString &inputtype,
                                 const uint audioratelimit,
                                 const QString &hostname,
                                 const uint dvb_swfilter,
                                 const uint dvb_sat_type,
                                 bool       dvb_wait_for_seqstart,
                                 bool       skipbtaudio,
                                 bool       dvb_on_demand,
                                 const uint dvb_diseqc_type,
                                 const uint firewire_speed,
                                 const QString &firewire_model,
                                 const uint firewire_connection,
                                 const uint signal_timeout,
                                 const uint channel_timeout,
                                 const uint dvb_tuning_delay,
                                 const uint contrast,
                                 const uint brightness,
                                 const uint colour,
                                 const uint hue,
                                 const uint diseqcid,
                                 bool       dvb_eitscan)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "INSERT INTO capturecard "
        "(videodevice, audiodevice, vbidevice, cardtype, "
        "audioratelimit, hostname, dvb_swfilter, dvb_sat_type, "
        "dvb_wait_for_seqstart, skipbtaudio, dvb_on_demand, dvb_diseqc_type, "
        "firewire_speed, firewire_model, firewire_connection, signal_timeout, "
        "channel_timeout, dvb_tuning_delay, contrast, brightness, colour, "
        "hue, diseqcid, dvb_eitscan) "
        "VALUES (:VIDEODEVICE, :AUDIODEVICE, :VBIDEVICE, :INPUTTYPE, "
        ":AUDIORATELIMIT, :HOSTNAME, :DVBSWFILTER, :DVBSATTYPE, "
        ":DVBWAITFORSEQSTART, :SKIPBTAUDIO, :DVBONDEMAND, :DVBDISEQCTYPE, "
        ":FIREWIRESPEED, :FIREWIREMODEL, :FIREWIRECONNECTION, :SIGNALTIMEOUT, "
        ":CHANNELTIMEOUT, :DVBTUNINGDELAY, :CONTRAST, :BRIGHTNESS, :COLOUR, "
        ":HUE, :DISEQCID, :DVBEITSCAN ) ");

    query.bindValue(":VIDEODEVICE", videodevice);
    query.bindValue(":AUDIODEVICE", audiodevice);
    query.bindValue(":VBIDEVICE", vbidevice);
    query.bindValue(":INPUTTYPE", inputtype);
    query.bindValue(":AUDIORATELIMIT", audioratelimit);
    query.bindValue(":HOSTNAME", hostname);
    query.bindValue(":DVBSWFILTER", dvb_swfilter);
    query.bindValue(":DVBSATTYPE", dvb_sat_type);
    query.bindValue(":DVBWAITFORSEQSTART", dvb_wait_for_seqstart);
    query.bindValue(":SKIPBTAUDIO", skipbtaudio);
    query.bindValue(":DVBONDEMAND", dvb_on_demand);
    query.bindValue(":DVBDISEQCTYPE", dvb_diseqc_type);
    query.bindValue(":FIREWIRESPEED", firewire_speed);
    query.bindValue(":FIREWIREMODEL", firewire_model);
    query.bindValue(":FIREWIRECONNECTION", firewire_connection);
    query.bindValue(":SIGNALTIMEOUT", signal_timeout);
    query.bindValue(":CHANNELTIMEOUT", channel_timeout);
    query.bindValue(":DVBTUNINGDELAY", dvb_tuning_delay);
    query.bindValue(":CONTRAST", contrast);
    query.bindValue(":BRIGHTNESS", brightness);
    query.bindValue(":COLOUR", colour);
    query.bindValue(":HUE", hue);
    query.bindValue(":DISEQCID", diseqcid);
    query.bindValue(":DVBEITSCAN", dvb_eitscan);

    if (!query.exec())
    {
        MythDB::DBError("CreateCaptureCard", query);
        return -1;
    }

    query.prepare("SELECT MAX(cardid) FROM capturecard");

    if (!query.exec())
    {
        MythDB::DBError("CreateCaptureCard maxinput", query);
        return -1;
    }

    int inputid = -1;  /* must be int not uint because of return type. */

    if (query.next())
    {
        inputid = query.value(0).toInt();
        uint groupid = CardUtil::CreateDeviceInputGroup(inputid, inputtype,
                                                        hostname, videodevice);
        CardUtil::LinkInputGroup(inputid, groupid);
    }

    return inputid;
}

bool CardUtil::DeleteInput(uint inputid)
{
    vector<uint> childids = GetChildInputIDs(inputid);
    for (uint i = 0; i < childids.size(); ++i)
    {
        if (!DeleteInput(childids[i]))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CardUtil: Failed to delete child input %1")
                .arg(childids[i]));
            return false;
        }
    }

    MSqlQuery query(MSqlQuery::InitCon());

    DiSEqCDevTree tree;
    tree.Load(inputid);

    // Delete the capturecard row for this input
    query.prepare("DELETE FROM capturecard WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    if (!query.exec())
    {
        MythDB::DBError("DeleteCard -- delete capturecard", query);
        return false;
    }

    // Update the reclimit of the parent input
    query.prepare("UPDATE capturecard SET reclimit=reclimit-1 "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    if (!query.exec())
    {
        MythDB::DBError("DeleteCard -- update capturecard", query);
        return false;
    }

    // Delete the inputgroup rows for this input
    query.prepare("DELETE FROM inputgroup WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    if (!query.exec())
    {
        MythDB::DBError("DeleteCard -- delete inputgroup", query);
        return false;
    }

    // Delete the diseqc tree if no more inputs reference it.
    if (tree.Root())
    {
        query.prepare("SELECT cardid FROM capturecard "
                      "WHERE diseqcid = :DISEQCID LIMIT 1");
        query.bindValue(":DISEQCID", tree.Root()->GetDeviceID());
        if (!query.exec())
        {
            MythDB::DBError("DeleteCard -- find diseqc tree", query);
        }
        else if (!query.next())
        {
            tree.SetRoot(NULL);
            tree.Store(inputid);
        }
    }

    // delete any unused input groups
    UnlinkInputGroup(0, 0);

    return true;
}

bool CardUtil::DeleteAllInputs(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    return (query.exec("TRUNCATE TABLE inputgroup") &&
            query.exec("TRUNCATE TABLE diseqc_config") &&
            query.exec("TRUNCATE TABLE diseqc_tree") &&
            query.exec("TRUNCATE TABLE capturecard") &&
            query.exec("TRUNCATE TABLE iptv_channel"));
}

vector<uint> CardUtil::GetInputList(void)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "ORDER BY cardid");

    if (!query.exec())
        MythDB::DBError("CardUtil::GetInputList()", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

vector<uint> CardUtil::GetSchedInputList(void)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM capturecard "
        "WHERE schedorder <> 0 "
        "ORDER BY schedorder, cardid");

    if (!query.exec())
        MythDB::DBError("CardUtil::GetSchedInputList()", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

vector<uint> CardUtil::GetLiveTVInputList(void)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM capturecard "
        "WHERE livetvorder <> 0 "
        "ORDER BY livetvorder, cardid");

    if (!query.exec())
        MythDB::DBError("CardUtil::GetLiveTVInputList()", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

QString CardUtil::GetDeviceName(dvb_dev_type_t type, const QString &device)
{
    QString devname = QString(device);

    if (DVB_DEV_FRONTEND == type)
        return devname;
    else if (DVB_DEV_DVR == type)
        return devname.replace(devname.indexOf("frontend"), 8, "dvr");
    else if (DVB_DEV_DEMUX == type)
        return devname.replace(devname.indexOf("frontend"), 8, "demux");
    else if (DVB_DEV_CA == type)
        return devname.replace(devname.indexOf("frontend"), 8, "ca");
    else if (DVB_DEV_AUDIO == type)
        return devname.replace(devname.indexOf("frontend"), 8, "audio");
    else if (DVB_DEV_VIDEO == type)
        return devname.replace(devname.indexOf("frontend"), 8, "video");

    return "";
}

/**
 * If the device is valid, check if the model does DVB.
 *
 * \todo Replace with a more general purpose routine - something that gets
 *       /sys/features and searches that for particular modulation types. e.g.
 *       bool CardUtil::DoesHDHRsupport(const QString     &device,
 *                                      const QStringList &modTypes);
 */

bool CardUtil::HDHRdoesDVB(const QString &device)
{
    (void) device;

#ifdef USING_HDHOMERUN
    hdhomerun_device_t  *hdhr;
    hdhr = hdhomerun_device_create_from_str(device.toLatin1(), NULL);
    if (!hdhr)
        return false;

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (model && strstr(model, "dvb"))
    {
        hdhomerun_device_destroy(hdhr);
        return true;
    }

    hdhomerun_device_destroy(hdhr);

#endif

    return false;
}

/**
 * Get a nicely formatted string describing the device
 */

QString CardUtil::GetHDHRdesc(const QString &device)
{
    QString connectErr = QObject::tr("Unable to connect to device.");

#ifdef USING_HDHOMERUN
    bool      deviceIsIP = false;
    uint32_t  dev;

    if (device.contains('.'))  // Simplistic check, but also allows DNS names
        deviceIsIP = true;
    else
    {
        bool validID;

        dev = device.toUInt(&validID, 16);
        if (!validID || !hdhomerun_discover_validate_device_id(dev))
            return QObject::tr("Invalid Device ID");
    }
    (void) deviceIsIP;

    LOG(VB_GENERAL, LOG_INFO, "CardUtil::GetHDHRdescription(" + device +
                              ") - trying to locate device");

    hdhomerun_device_t  *hdhr;
    hdhr = hdhomerun_device_create_from_str(device.toLatin1(), NULL);
    if (!hdhr)
        return QObject::tr("Invalid Device ID or address.");

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (!model)
    {
        hdhomerun_device_destroy(hdhr);
        return connectErr;
    }


    QString   description = model;
    char     *sVersion;
    uint32_t  iVersion;

    if (hdhomerun_device_get_version(hdhr, &sVersion, &iVersion))
        description += QObject::tr(", firmware: %2").arg(sVersion);

    hdhomerun_device_destroy(hdhr);

    return description;
#else

    (void) device;
    return connectErr;
#endif
}

/**
 * Get a nicely formatted string describing the device
 */

QString CardUtil::GetVBoxdesc(const QString &id, const QString &ip,
                              const QString &tunerNo, const QString &tunerType)
{
    QString connectErr = QObject::tr("Unable to connect to device.");

#ifdef USING_VBOX
    VBox *vbox = new VBox(ip);

    if (!vbox->checkConnection())
    {
        delete vbox;
        return connectErr;
    }

    QString version;

    if (!vbox->checkVersion(version))
    {
        QString apiVersionErr = QObject::tr("The VBox software version is too old (%1), we require %2")
                                            .arg(version).arg(VBOX_MIN_API_VERSION);
        delete vbox;
        return apiVersionErr;

    }

    delete vbox;

    return QString("V@Box TV Gateway - ID: %1, IP: %2, Tuner: %3-%4").arg(id)
                   .arg(ip).arg(tunerNo).arg(tunerType);

#else
    (void) id;
    (void) ip;
    (void) tunerNo;
    (void) tunerType;

    return connectErr;
#endif
}

#ifdef USING_ASI
static QString sys_dev(uint device_num, QString dev)
{
    return QString("/sys/class/asi/asirx%1/%2").arg(device_num).arg(dev);
}

static QString read_sys(QString sys_dev)
{
    QFile f(sys_dev);
    f.open(QIODevice::ReadOnly);
    QByteArray sdba = f.readAll();
    f.close();
    return sdba;
}

static bool write_sys(QString sys_dev, QString str)
{
    QFile f(sys_dev);
    f.open(QIODevice::WriteOnly);
    QByteArray ba = str.toLocal8Bit();
    qint64 offset = 0;
    for (uint tries = 0; (offset < ba.size()) && tries < 5; tries++)
    {
        qint64 written = f.write(ba.data()+offset, ba.size()-offset);
        if (written < 0)
            return false;
        offset += written;
    }
    return true;
}
#endif

int CardUtil::GetASIDeviceNumber(const QString &device, QString *error)
{
#ifdef USING_ASI
    // basic confirmation
    struct stat statbuf;
    memset(&statbuf, 0, sizeof(statbuf));
    if (stat(device.toLocal8Bit().constData(), &statbuf) < 0)
    {
        if (error)
            *error = QString("Unable to stat '%1'").arg(device) + ENO;
        return -1;
    }

    if (!S_ISCHR(statbuf.st_mode))
    {
        if (error)
            *error = QString("'%1' is not a character device").arg(device);
        return -1;
    }

    if (!(statbuf.st_rdev & 0x0080))
    {
        if (error)
            *error = QString("'%1' not a DVEO ASI receiver").arg(device);
        return -1;
    }

    int device_num = statbuf.st_rdev & 0x007f;

    // extra confirmation
    QString sys_dev_contents = read_sys(sys_dev(device_num, "dev"));
    QStringList sys_dev_clist = sys_dev_contents.split(":");
    if (2 != sys_dev_clist.size())
    {
        if (error)
        {
            *error = QString("Unable to read '%1'")
                .arg(sys_dev(device_num, "dev"));
        }
        return -1;
    }
    if (sys_dev_clist[0].toUInt() != (statbuf.st_rdev>>8))
    {
        if (error)
            *error = QString("'%1' not a DVEO ASI device").arg(device);
        return -1;
    }

    return device_num;
#else
    (void) device;
    if (error)
        *error = "Not compiled with ASI support.";
    return -1;
#endif
}

uint CardUtil::GetASIBufferSize(uint device_num, QString *error)
{
#ifdef USING_ASI
    // get the buffer size
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "bufsize"));
    bool ok;
    uint buf_size = sys_bufsize_contents.toUInt(&ok);
    if (!ok)
    {
        if (error)
        {
            *error = QString("Failed to read buffer size from '%1'")
                .arg(sys_dev(device_num, "bufsize"));
        }
        return 0;
    }
    return buf_size;
#else
    (void) device_num;
    if (error)
        *error = "Not compiled with ASI support.";
    return 0;
#endif
}

uint CardUtil::GetASINumBuffers(uint device_num, QString *error)
{
#ifdef USING_ASI
    // get the buffer size
    QString sys_numbuffers_contents = read_sys(sys_dev(device_num, "buffers"));
    bool ok;
    uint num_buffers = sys_numbuffers_contents.toUInt(&ok);
    if (!ok)
    {
        if (error)
        {
            *error = QString("Failed to read num buffers from '%1'")
                .arg(sys_dev(device_num, "buffers"));
        }
        return 0;
    }
    return num_buffers;
#else
    (void) device_num;
    if (error)
        *error = "Not compiled with ASI support.";
    return 0;
#endif
}

int CardUtil::GetASIMode(uint device_num, QString *error)
{
#ifdef USING_ASI
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "mode"));
    bool ok;
    uint mode = sys_bufsize_contents.toUInt(&ok);
    if (!ok)
    {
        if (error)
        {
            *error = QString("Failed to read mode from '%1'")
                .arg(sys_dev(device_num, "mode"));
        }
        return -1;
    }
    return mode;
#else
    (void) device_num;
    if (error)
        *error = "Not compiled with ASI support.";
    return -1;
#endif
}

bool CardUtil::SetASIMode(uint device_num, uint mode, QString *error)
{
#ifdef USING_ASI
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "mode"));
    bool ok;
    uint old_mode = sys_bufsize_contents.toUInt(&ok);
    if (ok && old_mode == mode)
        return true;
    ok = write_sys(sys_dev(device_num, "mode"), QString("%1\n").arg(mode));
    if (!ok && error)
    {
        *error = QString("Failed to set mode to %1 using '%2'")
            .arg(mode).arg(sys_dev(device_num, "mode"));
    }
    return ok;
#else
    Q_UNUSED(device_num);
    Q_UNUSED(mode);
    if (error)
        *error = "Not compiled with ASI support.";
    return false;
#endif
}

/** \fn CardUtil::IsVBoxPresent(uint inputid)
 *  \brief Returns true if the VBox responds to a ping
 *  \param inputid  Inputid  as used in DB capturecard table
 */
bool CardUtil::IsVBoxPresent(uint inputid)
{
    // should only be called if inputtype == VBOX
    if (!inputid )
    {
        LOG(VB_GENERAL, LOG_ERR, QString("VBOX inputid  (%1) not valid, redo mythtv-setup")
                .arg(inputid));
        return false;
    }

    // get sourceid and startchan from table capturecard for inputid
    uint chanid = 0;
    chanid = ChannelUtil::GetChannelValueInt("chanid",GetSourceID(inputid),GetStartingChannel(inputid));
    if (!chanid)
    {
        // no chanid, presume bad setup
        LOG(VB_GENERAL, LOG_ERR, QString("VBOX chanid  (%1) not found for inputid (%2) , redo mythtv-setup")
                .arg(chanid).arg(inputid));
        return false;
    }

    // get timeouts for inputid
    uint signal_timeout = 0;
    uint tuning_timeout = 0;
    if (!GetTimeouts(inputid,signal_timeout,tuning_timeout))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to get timeouts for inputid (%1)")
                .arg(inputid));
        return false;
    }

    signal_timeout = signal_timeout/1000; //convert to seconds

    // now get url from iptv_channel table
    QUrl url;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT url "
                  "FROM iptv_channel "
                  "WHERE chanid = :CHANID");
    query.bindValue(":CHANID", chanid);

    if (!query.exec())
        MythDB::DBError("CardUtil::IsVBoxPresent url", query);
    else if (query.next())
        url = query.value(0).toString();

    //now get just the IP address from the url
    QString ip ="";
    ip = url.host();
    LOG(VB_GENERAL, LOG_INFO, QString("VBOX IP found (%1) for inputid (%2)")
                .arg(ip).arg(inputid));

    if (!ping(ip,signal_timeout))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("VBOX at IP  (%1) failed to respond to network ping for inputid (%2) timeout (%3)")
                .arg(ip).arg(inputid).arg(signal_timeout));
        return false;
    }

    return true;
}

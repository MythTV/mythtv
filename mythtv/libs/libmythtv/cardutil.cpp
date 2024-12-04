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
#include <QRegularExpression>
#include <QUrlQuery>

// MythTV headers
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h" // for ping()

#include "cardutil.h"
#include "diseqcsettings.h"
#include "inputinfo.h"
#include "recorders/dvbchannel.h"
#include "sourceutil.h"
#include "videosource.h"

#ifdef USING_DVB
#include "recorders/dvbtypes.h"
#endif

#ifdef USING_V4L2
#include "v4l2util.h"
#endif

#ifdef USING_HDHOMERUN
#include HDHOMERUN_HEADERFILE
#endif

#ifdef USING_VBOX
#include "recorders/vboxutils.h"
#endif

#ifdef USING_SATIP
#include "recorders/satiputils.h"
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
    QStringList inputTypes {};

#ifdef USING_DVB
    inputTypes += "'DVB'";
#endif // USING_DVB

#ifdef USING_V4L2
    inputTypes += "'V4L'";
    inputTypes += "'MPEG'";
#endif // USING_V4L2

#ifdef USING_IPTV
    inputTypes += "'FREEBOX'";
#endif // USING_IPTV

#ifdef USING_VBOX
    inputTypes += "'VBOX'";
#endif // USING_VBOX

#ifdef USING_HDHOMERUN
    inputTypes += "'HDHOMERUN'";
#endif // USING_HDHOMERUN

#ifdef USING_SATIP
    inputTypes += "'SATIP'";
#endif // USING_SATIP

#ifdef USING_ASI
    inputTypes += "'ASI'";
#endif

#ifdef USING_CETON
    inputTypes += "'CETON'";
#endif // USING_CETON

#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    inputTypes += "'EXTERNAL'";
#endif

    if (inputTypes.isEmpty())
        return "('DUMMY')";

    return QString("(%1)").arg(inputTypes.join(','));
}

bool CardUtil::IsCableCardPresent([[maybe_unused]] uint inputid,
                                  const QString &inputType)
{
    if (inputType == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        hdhomerun_tuner_status_t status {};
        QString device = GetVideoDevice(inputid);
        hdhomerun_device_t *hdhr =
            hdhomerun_device_create_from_str(device.toLatin1(), nullptr);
        if (!hdhr)
            return false;

        int oob = hdhomerun_device_get_oob_status(hdhr, nullptr, &status);

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
    if (inputType == "CETON")
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

        const QString& ip_address = parts.at(0);

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

        auto *request = new QNetworkRequest();
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

        static const QRegularExpression regex { "^\\{ \"?result\"?: \"(.*)\" \\}$"};
        auto match = regex.match(response);
        if (!match.hasMatch())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("CardUtil: Ceton unexpected http response: %1").arg(response));
            return false;
        }

        QString result = match.captured(1);

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
    return false;
}

bool CardUtil::HasTuner(const QString &rawtype,
                        [[maybe_unused]] const QString & device)
{
    if (rawtype == "DVB"     || rawtype == "HDHOMERUN" ||
        rawtype == "FREEBOX" || rawtype == "CETON" ||
        rawtype == "VBOX"    || rawtype == "SATIP")
        return true;

#ifdef USING_V4L2
    if (rawtype == "V4L2ENC")
    {
        V4L2util v4l2(device);
        return !v4l2 ? false : v4l2.HasTuner();
    }
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
        MythDB::DBError("CardUtil::is_tuner_shared()", query);
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
        MythDB::DBError("CardUtil::IsInputTypePresent()", query);
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
            {
                inputtypes[cardtype] = "";
            }
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

QMap <QString,QStringList> CardUtil::s_videoDeviceCache;

void CardUtil::ClearVideoDeviceCache()
{
    s_videoDeviceCache.clear();
}


QStringList CardUtil::ProbeVideoDevices(const QString &rawtype)
{
    if (s_videoDeviceCache.contains(rawtype))
        return s_videoDeviceCache[rawtype];

    QStringList devs;

    if (rawtype.toUpper() == "DVB")
    {
        QDir dir("/dev/dvb", "adapter*", QDir::Name, QDir::Dirs);
        QFileInfoList entries = dir.entryInfoList();
        for (const auto & it : std::as_const(entries))
        {
            QDir subdir(it.filePath(), "frontend*", QDir::Name, QDir::Files | QDir::System);
            const QFileInfoList subil = subdir.entryInfoList();
            if (subil.isEmpty())
                continue;

            for (const auto & subit : std::as_const(subil))
                devs.push_back(subit.filePath());
        }
    }
    else if (rawtype.toUpper() == "ASI")
    {
        QDir dir("/dev/", "asirx*", QDir::Name, QDir::System);
        QFileInfoList entries = dir.entryInfoList();
        for (const auto & it : std::as_const(entries))
        {
            if (GetASIDeviceNumber(it.filePath()) >= 0)
            {
                devs.push_back(it.filePath());
                continue;
            }
            break;
        }
    }
#ifdef USING_HDHOMERUN
    else if (rawtype.toUpper() == "HDHOMERUN")
    {
#if HDHOMERUN_VERSION >= 20221010
        struct hdhomerun_debug_t *dbg = hdhomerun_debug_create();
        struct hdhomerun_discover_t *ds = hdhomerun_discover_create(dbg);

        // Find devices
        uint32_t type  { HDHOMERUN_DEVICE_TYPE_TUNER };
        uint32_t flags { HDHOMERUN_DISCOVER_FLAGS_IPV4_GENERAL |
                         HDHOMERUN_DISCOVER_FLAGS_IPV6_GENERAL };
        int result =
            hdhomerun_discover2_find_devices_broadcast(ds, flags, &type, 1);

        if (result == -1)
        {
            LOG(VB_GENERAL, LOG_ERR, "Error finding HDHomerun devices");
        }
        else if (result == 0)
        {
            LOG(VB_GENERAL, LOG_INFO, "No HDHomerun devices found.");
        }
        else
        {
            // For each device found
            struct hdhomerun_discover2_device_t *device = hdhomerun_discover2_iter_device_first(ds);
            while (device)
            {
                uint8_t tuners = hdhomerun_discover2_device_get_tuner_count(device);
                uint32_t device_id = hdhomerun_discover2_device_get_device_id(device);
                QString id = QString("%1").arg(device_id, 0, 16, QChar('0')).toUpper();
                auto *dev1 = hdhomerun_device_create_from_str(id.toLatin1(), nullptr);
                QString model = hdhomerun_device_get_model_str(dev1);

                QHostAddress ip;
                struct sockaddr_storage saddr {};
                struct hdhomerun_discover2_device_if_t *device_if { nullptr };

                // Enumerate all addresses
                device_if = hdhomerun_discover2_iter_device_if_first(device);
                while (device_if)
                {
                    hdhomerun_discover2_device_if_get_ip_addr(device_if, &saddr);
                    ip = QHostAddress((struct sockaddr *)&saddr);
                    LOG(VB_GENERAL, LOG_DEBUG,
                        QString("HDHomerun %1 has IP %2").arg(id, ip.toString()));
                    device_if = hdhomerun_discover2_iter_device_if_next(device_if);
                }

                // HDHomerun API recommends using first entry
                device_if = hdhomerun_discover2_iter_device_if_first(device);
                if (nullptr == device_if)
                {
                    LOG(VB_GENERAL, LOG_WARNING,
                        QString("HDHomerun %1 has no IP addresses").arg(id));
                    continue;
                }
                hdhomerun_discover2_device_if_get_ip_addr(device_if, &saddr);
                ip = QHostAddress((struct sockaddr *)&saddr);

                // Create device name
                QString hdhrdev = QString("%1 %2 %3").arg(id, ip.toString(), model);
                devs.push_back(hdhrdev);
                LOG(VB_GENERAL, LOG_INFO,
                    QString("HDHomerun %1: IP %2, model %3, %4 tuners")
                    .arg(id, ip.toString(), model).arg(tuners));

                device = hdhomerun_discover2_iter_device_next(device);
            }
            hdhomerun_discover_destroy(ds);
            hdhomerun_debug_destroy(dbg);
        }
#else // HDHOMERUN_VERSION >= 20221010
        uint32_t  target_ip   = 0;
        uint32_t  device_type = HDHOMERUN_DEVICE_TYPE_TUNER;
        uint32_t  device_id   = HDHOMERUN_DEVICE_ID_WILDCARD;
        const int max_count   = 50;
        std::array<hdhomerun_discover_device_t,max_count> result_list {};

        int result = hdhomerun_discover_find_devices_custom_v2(
            target_ip, device_type, device_id, result_list.data(), result_list.size());

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

            QString model = "";
            hdhomerun_device_t *device = hdhomerun_device_create(
                result_list[i].device_id, 0, 0, nullptr);
            if (device)
            {
                model = hdhomerun_device_get_model_str(device);
                hdhomerun_device_destroy(device);
            }

            QString hdhrdev = id.toUpper() + " " + ip + " " + model;
            devs.push_back(hdhrdev);
        }
#endif // HDHOMERUN_VERSION >= 20221010
    }
#endif // USING_HDHOMERUN
#ifdef USING_SATIP
    else if (rawtype.toUpper() == "SATIP")
    {
        devs = SatIP::probeDevices();
    }
#endif // USING_SATIP
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

    s_videoDeviceCache.insert(rawtype,devs);
    return devs;
}

// Get the list of delivery systems from the card
QStringList CardUtil::ProbeDeliverySystems([[maybe_unused]] const QString &device)
{
    QStringList delsyslist;

#ifdef USING_DVB
    int fd_frontend = OpenVideoDevice(device);
    if (fd_frontend < 0)
    {
        return delsyslist;
    }

    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_API_VERSION;
    cmd.num = 1;
    cmd.props = &prop;
    if (ioctl(fd_frontend, FE_GET_PROPERTY, &cmd) == 0)
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("CardUtil(%1): ").arg(device) +
            QString("dvb api version %1.%2").arg((prop.u.data>>8)&0xff).arg((prop.u.data)&0xff));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("CardUtil(%1) FE_GET_PROPERTY ioctl failed").arg(device) + ENO);
        close(fd_frontend);
        return delsyslist;
    }

    delsyslist = ProbeDeliverySystems(fd_frontend);

    QString msg = "Delivery systems:";
    for (const auto & item : std::as_const(delsyslist))
    {
        msg += " ";
        msg += item;
    }
    LOG(VB_GENERAL, LOG_INFO, QString("CardUtil(%1): ").arg(device) + msg);

    close(fd_frontend);
#endif  // USING_DVB

    return delsyslist;
}

// Get the list of all supported delivery systems from the card
QStringList CardUtil::ProbeDeliverySystems([[maybe_unused]] int fd_frontend)
{
    QStringList delsyslist;

#ifdef USING_DVB
    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_ENUM_DELSYS;
    cmd.num = 1;
    cmd.props = &prop;
    if (ioctl(fd_frontend, FE_GET_PROPERTY, &cmd) == 0)
    {
        for (unsigned int i = 0; i < prop.u.buffer.len; i++)
        {
            delsyslist.push_back(DTVModulationSystem::toString(prop.u.buffer.data[i]));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "FE_GET_PROPERTY ioctl failed " + ENO);
    }
#endif  // USING_DVB

    return delsyslist;
}

QString CardUtil::ProbeDefaultDeliverySystem([[maybe_unused]] const QString &device)
{
    DTVModulationSystem delsys;

#ifdef USING_DVB
    int fd = OpenVideoDevice(device);
    if (fd >= 0)
    {
        delsys = ProbeBestDeliverySystem(fd);
        close(fd);
    }
#endif  // USING_DVB

    return delsys.toString();
}

QString CardUtil::ProbeDVBType(const QString &device)
{
    QString ret = "ERROR_UNKNOWN";

    if (device.isEmpty())
        return ret;

#ifdef USING_DVB
    DTVTunerType type = ProbeTunerType(device);
    ret = (type.toString() != "UNKNOWN") ? type.toString().toUpper() : ret;

    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("(%1) tuner type:%2 %3")
        .arg(device).arg(type).arg(ret));
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::ProbeDVBFrontendName(const QString &)
 *  \brief Returns the input type from the video device
 */
QString CardUtil::ProbeDVBFrontendName([[maybe_unused]] const QString &device)
{
    QString ret = "ERROR_UNKNOWN";

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toLatin1();
    int fd_frontend = open(dev.constData(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
        return "ERROR_OPEN";

    struct dvb_frontend_info info {};
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

std::chrono::milliseconds CardUtil::GetMinSignalMonitoringDelay(const QString &device)
{
    QString name = ProbeDVBFrontendName(device);
    if (name.indexOf("DVB-S") >= 0)
        return 300ms;
    if (name == "DiBcom 3000P/M-C DVB-T")
        return 100ms;
    return 25ms;
}

DTVTunerType CardUtil::ConvertToTunerType(DTVModulationSystem delsys)
{
    DTVTunerType tunertype;

    switch (delsys)
    {
        case DTVModulationSystem::kModulationSystem_DVBS:
            tunertype = DTVTunerType::kTunerTypeDVBS1;
            break;
        case DTVModulationSystem::kModulationSystem_DVBS2:
            tunertype = DTVTunerType::kTunerTypeDVBS2;
            break;
        case DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A:
        case DTVModulationSystem::kModulationSystem_DVBC_ANNEX_C:
            tunertype = DTVTunerType::kTunerTypeDVBC;
            break;
        case DTVModulationSystem::kModulationSystem_DVBT:
            tunertype = DTVTunerType::kTunerTypeDVBT;
            break;
        case DTVModulationSystem::kModulationSystem_DVBT2:
            tunertype = DTVTunerType::kTunerTypeDVBT2;
            break;
        case DTVModulationSystem::kModulationSystem_DTMB:
            tunertype = DTVTunerType::kTunerTypeDVBT;
            break;
        case DTVModulationSystem::kModulationSystem_ATSC:
        case DTVModulationSystem::kModulationSystem_DVBC_ANNEX_B:
            tunertype = DTVTunerType::kTunerTypeATSC;
            break;
        case DTVModulationSystem::kModulationSystem_UNDEFINED:
            tunertype = DTVTunerType::kTunerTypeUnknown;
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("TODO Add to switch case delivery system:%2 %3")
                    .arg(delsys).arg(delsys.toString()));
            break;
    }

    return tunertype;
}

// Get the currently configured tuner type from the database
DTVTunerType CardUtil::GetTunerType(uint inputid)
{
    DTVModulationSystem delsys = GetDeliverySystem(inputid);
    DTVTunerType tunertype = ConvertToTunerType(delsys);
    return tunertype;
}

// Get the currently configured tuner type from the device
DTVTunerType CardUtil::ProbeTunerType(int fd_frontend)
{
    DTVModulationSystem delsys = ProbeCurrentDeliverySystem(fd_frontend);
    DTVTunerType tunertype = ConvertToTunerType(delsys);
    return tunertype;
}

// Get the currently configured tuner type from the device
DTVTunerType CardUtil::ProbeTunerType(const QString &device)
{
    DTVModulationSystem delsys = ProbeCurrentDeliverySystem(device);
    DTVTunerType tunertype = ConvertToTunerType(delsys);
    return tunertype;
}

// Get the tuner type from the multiplex
DTVTunerType CardUtil::GetTunerTypeFromMultiplex(uint mplexid)
{
    DTVTunerType tuner_type;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT mod_sys "
        "FROM dtv_multiplex "
        "WHERE dtv_multiplex.mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetTunerTypeFromMultiplex", query);
        return tuner_type;
    }

    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not find entry in dtv_multiplex for mplexid %1")
                .arg(mplexid));

        return tuner_type;
    }

    DTVModulationSystem mod_sys;
    mod_sys.Parse(query.value(0).toString());
    tuner_type = CardUtil::ConvertToTunerType(mod_sys);

    return tuner_type;
}

// Get the currently configured delivery system from the database
DTVModulationSystem CardUtil::GetDeliverySystem(uint inputid)
{
    QString ds = GetDeliverySystemFromDB(inputid);
    DTVModulationSystem delsys;
    delsys.Parse(ds);
    return delsys;
}

// Get the currently configured delivery system from the device
DTVModulationSystem CardUtil::ProbeCurrentDeliverySystem([[maybe_unused]] const QString &device)
{
    DTVModulationSystem delsys;

    if (device.isEmpty())
    {
        return delsys;
    }

#ifdef USING_DVB
    int fd_frontend = OpenVideoDevice(device);
    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("open failed (%1)")
                .arg(device) + ENO);
        return delsys;
    }

    delsys = ProbeCurrentDeliverySystem(fd_frontend);

    LOG(VB_GENERAL, LOG_DEBUG, QString("CardUtil(%1): delsys:%2 %3")
        .arg(device).arg(delsys).arg(delsys.toString()));

    close(fd_frontend);
#endif

    return delsys;
}

// Get the currently configured delivery system from the device
DTVModulationSystem CardUtil::ProbeCurrentDeliverySystem([[maybe_unused]] int fd_frontend)
{
    DTVModulationSystem delsys;

#ifdef USING_DVB
    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_DELIVERY_SYSTEM;
    // prop.u.data = delsys;
    cmd.num = 1;
    cmd.props = &prop;

    int ret = ioctl(fd_frontend, FE_GET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("FE_GET_PROPERTY ioctl failed (fd_frontend:%1)")
                .arg(fd_frontend) + ENO);
        return delsys;
    }

    delsys = prop.u.data;

#endif // USING_DVB

    return delsys;
}

// Get the delivery system from database table capturecard.
// If there is nothing in the database then get the currently
// configured delivery system, check for DVB-T/T2 and DVB-S/S2
// and update the database.
// Configure the tuner.
// Return the tuner type corresponding with the modulation system.
QString CardUtil::ProbeSubTypeName(uint inputid)
{
    QString type = GetRawInputType(inputid);
    if ("DVB" != type)
        return type;

    DTVTunerType tunertype;
    int fd_frontend = OpenVideoDevice(inputid);
    if (fd_frontend < 0)
        return "ERROR_OPEN";

    DTVModulationSystem delsys = GetDeliverySystem(inputid);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED == delsys)
    {
        delsys = ProbeBestDeliverySystem(fd_frontend);
        if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
        {
            // Update database
            set_on_input("inputname", inputid, delsys.toString());      // Update DB capturecard
            LOG(VB_GENERAL, LOG_INFO,
                QString("CardUtil[%1]: ").arg(inputid) +
                QString("Update capturecard delivery system: %1").arg(delsys.toString()));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("CardUtil[%1]: Error probing best delivery system").arg(inputid));
            return "ERROR_UNKNOWN";
        }
    }
    SetDeliverySystem(inputid, delsys, fd_frontend);
    tunertype = ConvertToTunerType(delsys);
    close(fd_frontend);

    QString subtype = "ERROR_UNKNOWN";
    if (DTVTunerType::kTunerTypeUnknown != tunertype)
    {
        subtype = tunertype.toString();
    }

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("CardUtil[%1]: subtype:%2").arg(inputid).arg(subtype));

    return subtype;
}

/// \brief Returns true iff the input_type is one of the DVB types.
bool CardUtil::IsDVBInputType(const QString &inputType)
{
    QString t = inputType.toUpper();
    return (t == "DVB") || (t == "QPSK") || (t == "QAM") || (t == "OFDM") ||
        (t == "ATSC") || (t == "DVB_S2") || (t == "DVB_T2");
}

// Get the current delivery system from the card
// Get all supported delivery systems from the card
// If the current delivery system is DVB-T and DVB-T2 is supported then select DVB-T2
// If the current delivery system is DVB-S and DVB-S2 is supported then select DVB-S2
//
DTVModulationSystem CardUtil::ProbeBestDeliverySystem([[maybe_unused]] int fd)
{
    DTVModulationSystem delsys;

#ifdef USING_DVB
    // Get the current delivery system from the card
    delsys = ProbeCurrentDeliverySystem(fd);
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Current delivery system: %1").arg(delsys.toString()));

    // Get all supported delivery systems from the card
    QString msg = "Supported delivery systems:";
    QStringList delsyslist = ProbeDeliverySystems(fd);
    msg.append(delsyslist.join(" "));
    LOG(VB_GENERAL, LOG_DEBUG, LOC + msg);

    // If the current delivery system is DVB-T and DVB-T2 is supported then select DVB-T2
    if (DTVModulationSystem::kModulationSystem_DVBT == delsys)
    {
        DTVModulationSystem newdelsys(DTVModulationSystem::kModulationSystem_DVBT2);
        if (delsyslist.contains(newdelsys.toString()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Changing delivery system from %1 to %2")
                    .arg(delsys.toString(), newdelsys.toString()));
            delsys = newdelsys;
        }
    }

    // If the current delivery system is DVB-S and DVB-S2 is supported then select DVB-S2
    if (DTVModulationSystem::kModulationSystem_DVBS == delsys)
    {
        DTVModulationSystem newdelsys(DTVModulationSystem::kModulationSystem_DVBS2);
        if (delsyslist.contains(newdelsys.toString()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Changing delivery system from %1 to %2")
                    .arg(delsys.toString(), newdelsys.toString()));
            delsys = newdelsys;
        }
    }
#endif

    return delsys;
}

// Get the delivery system from the database
// If not found then get the best delivery system from the card
//
DTVModulationSystem CardUtil::GetOrProbeDeliverySystem([[maybe_unused]] uint inputid,
                                                       [[maybe_unused]] int fd)
{
    DTVModulationSystem delsys;
#ifdef USING_DVB

    // If there is a valid modulation system in the database
    // then we return that and do nothing more.
    delsys = GetDeliverySystem(inputid);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
    {
        return delsys;
    }

    // Nothing in the database, get default and use that
    delsys = ProbeBestDeliverySystem(fd);
    LOG(VB_GENERAL, LOG_INFO,
        QString("CardUtil[%1]: ").arg(inputid) +
        QString("No capturecard delivery system in database, using: %1").arg(delsys.toString()));

#endif

    return delsys;
}

// Configure the tuner to use the delivery system from database
// If not found then set to the best delivery system from the card
//
int CardUtil::SetDefaultDeliverySystem([[maybe_unused]] uint inputid,
                                       [[maybe_unused]] int fd)
{
    int ret = -1;

#ifdef USING_DVB
    DTVModulationSystem delsys = GetOrProbeDeliverySystem(inputid, fd);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
    {
        ret = SetDeliverySystem(inputid, delsys, fd);
    }
#endif

    return ret;
}

// Set delivery system from database
//
int CardUtil::SetDeliverySystem([[maybe_unused]] uint inputid)
{
    int ret = -1;

#ifdef USING_DVB
    DTVModulationSystem delsys = GetDeliverySystem(inputid);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
    {
        ret = SetDeliverySystem(inputid, delsys);
    }
#endif // USING_DVB

    return ret;
}

int CardUtil::SetDeliverySystem([[maybe_unused]] uint inputid,
                                [[maybe_unused]] DTVModulationSystem delsys)
{
    int ret = -1;

#ifdef USING_DVB
    QString device = GetVideoDevice(inputid);

    if (device.isEmpty())
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("CardUtil[%1]: ").arg(inputid) +
            QString("inputid:%1 ").arg(inputid) +
            QString("delsys:%1").arg(delsys.toString()));
        return ret;
    }

    int fd_frontend = OpenVideoDevice(device);
    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("CardUtil[%1]: ").arg(inputid) +
            QString("open failed (%1)").arg(device) + ENO);
        return ret;
    }
    ret = SetDeliverySystem(inputid, delsys, fd_frontend);

    close(fd_frontend);
#endif // USING_DVB

    return ret;
}

// Get delivery system from the database and write it to the card
int CardUtil::SetDeliverySystem([[maybe_unused]] uint inputid,
                                [[maybe_unused]] int fd)
{
    int ret = -1;

#ifdef USING_DVB
    DTVModulationSystem delsys = GetDeliverySystem(inputid);
    if (DTVModulationSystem::kModulationSystem_UNDEFINED != delsys)
    {
        ret = SetDeliverySystem(inputid, delsys, fd);
    }
#endif // USING_DVB

    return ret;
}

// Write the delivery system to the card
int CardUtil::SetDeliverySystem([[maybe_unused]] uint inputid,
                                [[maybe_unused]] DTVModulationSystem delsys,
                                [[maybe_unused]] int fd)
{
    int ret = -1;

#ifdef USING_DVB
    LOG(VB_GENERAL, LOG_INFO,
        QString("CardUtil[%1]: ").arg(inputid) +
        QString("Set delivery system: %1").arg(delsys.toString()));

    struct dtv_property prop = {};
    struct dtv_properties cmd = {};

    prop.cmd = DTV_DELIVERY_SYSTEM;
    prop.u.data = delsys;
    cmd.num = 1;
    cmd.props = &prop;

    ret = ioctl(fd, FE_SET_PROPERTY, &cmd);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("[%1] FE_SET_PROPERTY ioctl failed")
                .arg(inputid) + ENO);
        return ret;
    }
#endif // USING_DVB

    return ret;
}

int CardUtil::OpenVideoDevice(int inputid)
{
    QString device = GetVideoDevice(inputid);
    return OpenVideoDevice(device);
}

int CardUtil::OpenVideoDevice(const QString &device)
{
    if (device.isEmpty())
        return -1;

    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toLatin1();
    int fd_frontend = open(dev.constData(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Can't open DVB frontend (%1) for %2.")
                .arg(dvbdev, device) + ENO);
    }
    return fd_frontend;
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
        MythDB::DBError("CardUtil::get_on_input", query);
    else if (query.next())
        return query.value(0).toString();

    return {};
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
std::vector<uint> CardUtil::GetInputIDs(const QString& videodevice,
                                   const QString& rawtype,
                                   const QString& inputname,
                                   QString hostname)
{
    std::vector<uint> list;

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

std::vector<uint> CardUtil::GetChildInputIDs(uint inputid)
{
    std::vector<uint> list;

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

    // Handle schedgroup and schedorder specially.  If schedgroup is
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
    std::vector<uint> src_grps = CardUtil::GetInputGroups(src_inputid);
    std::vector<uint> dst_grps = CardUtil::GetInputGroups(dst_inputid);
    for (uint dst_grp : dst_grps)
        CardUtil::UnlinkInputGroup(dst_inputid, dst_grp);
    for (uint src_grp : src_grps)
        CardUtil::LinkInputGroup(dst_inputid, src_grp);

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


bool CardUtil::InputSetMaxRecordings(uint parentid, uint max_recordings)
{
    if (max_recordings < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "InputSetMaxRecording: max must be greater than zero.");
        return false;
    }

    std::vector<uint> cardids = CardUtil::GetChildInputIDs(parentid);

    // Delete old clone cards as required.
    for (size_t i = cardids.size() + 1;
         (i > max_recordings) && !cardids.empty(); --i)
    {
        CardUtil::DeleteInput(cardids.back());
        cardids.pop_back();
    }

    // Clone this config to existing clone cards.
    for (uint id : cardids)
        CardUtil::CloneCard(parentid, id);

    // Create new clone cards as required.
    for (size_t i = cardids.size() + 1; i < max_recordings; ++i)
    {
        CardUtil::CloneCard(parentid, 0);
    }

    return true;
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

std::vector<uint> CardUtil::GetInputIDs(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM capturecard "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    std::vector<uint> list;

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputIDs(sourceid)", query);
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
        MythDB::DBError("CardUtil::SetStartChannel", query);
        return false;
    }

    return true;
}

bool CardUtil::GetInputInfo(InputInfo &input, std::vector<uint> *groupids)
{
    if (!input.m_inputId)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                  "inputname, sourceid, livetvorder, "
                  "schedorder, displayname, recpriority, quicktune "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", input.m_inputId);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputInfo()", query);
        return false;
    }

    if (!query.next())
        return false;

    input.m_name          = query.value(0).toString();
    input.m_sourceId      = query.value(1).toUInt();
    input.m_liveTvOrder   = query.value(2).toUInt();
    input.m_scheduleOrder = query.value(3).toUInt();
    input.m_displayName   = query.value(4).toString();
    input.m_recPriority   = query.value(5).toInt();
    input.m_quickTune     = query.value(6).toBool();

    if (groupids)
        *groupids = GetInputGroups(input.m_inputId);

    return true;
}

// virtTuners to include virtual tuners in the list
QList<InputInfo> CardUtil::GetAllInputInfo(bool virtTuners)
{
    QList<InputInfo> infoInputList;

    MSqlQuery query(MSqlQuery::InitCon());
    QString queryStr = "SELECT cardid, "
                  "inputname, sourceid, livetvorder, "
                  "schedorder, displayname, recpriority, quicktune "
                  "FROM capturecard";

    if (!virtTuners)
        queryStr.append(" WHERE parentid = 0");
    query.prepare(queryStr);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetAllInputInfo()", query);
        return infoInputList;
    }

    while (query.next())
    {
        InputInfo input;
        input.m_inputId       = query.value(0).toUInt();
        input.m_name          = query.value(1).toString();
        input.m_sourceId      = query.value(2).toUInt();
        input.m_liveTvOrder   = query.value(3).toUInt();
        input.m_scheduleOrder = query.value(4).toUInt();
        input.m_displayName   = query.value(5).toString();
        input.m_recPriority   = query.value(6).toInt();
        input.m_quickTune     = query.value(7).toBool();

        infoInputList.push_back(input);
    }

    return infoInputList;
}

QString CardUtil::GetInputName(uint inputid)
{
    InputInfo info("None", 0, inputid, 0, 0, 0);
    GetInputInfo(info);
    return info.m_name;
}

// Get start channel for a capture card
//
// The start channel is:
// - the last channel used for Live TV as found in field startchan
// - if that is not filled in, the first visible channel from
//   the video source of the capture card
//
// The start channel is maintained per capture card instance so
// different capture cards connected to the same video source
// and different multirec instances of the same capture card
// can have a different starting channel value.
//
QString CardUtil::GetStartChannel(uint inputid)
{
    QString startchan;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT startchan "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("CardUtil::GetStartChannel#1", query);
    }
    else if (query.next())
    {
        startchan = query.value(0).toString();
    }

    // Check if starting channel is valid; skip invisible channels.
    if (!startchan.isEmpty())
    {
        query.prepare("SELECT channel.chanid "
                      "FROM capturecard, channel "
                      "WHERE capturecard.cardid   = :INPUTID         AND "
                      "      capturecard.sourceid = channel.sourceid AND "
                      "      channel.deleted      IS NULL AND "
                      "      channel.visible      > 0     AND "
                      "      channel.channum      = :CHANNUM");
        query.bindValue(":INPUTID", inputid);
        query.bindValue(":CHANNUM", startchan);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("CardUtil::GetStartChannel#2", query);
        }
        else if (!query.next())
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("CardUtil[%1]: ").arg(inputid) +
                QString("Channel %1 on inputid %2 is invalid").arg(startchan).arg(inputid));
            startchan.clear();
        }
    }

    // If we do not have a start channel yet then use the first one available.
    // Any visible channel will do so there is no "order by" done.
    if (startchan.isEmpty())
    {
        query.prepare("SELECT channel.channum "
                      "FROM capturecard, channel "
                      "WHERE capturecard.cardid   = :INPUTID         AND "
                      "      capturecard.sourceid = channel.sourceid AND "
                      "      channel.deleted      IS NULL AND "
                      "      channel.visible      > 0 "
                      "LIMIT 1");
        query.bindValue(":INPUTID", inputid);

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("CardUtil::GetStartChannel#3", query);
        }
        else if (query.next())
        {
            startchan = query.value(0).toString();
        }
    }

    if (startchan.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING,
            QString("CardUtil[%1]: ").arg(inputid) +
            QString("No start channel found on inputid %1").arg(inputid));
    }
    else
    {
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("CardUtil[%1]: ").arg(inputid) +
            QString("Start channel %1 on inputid %2").arg(startchan).arg(inputid));
    }

    return startchan;
}

QString CardUtil::GetDisplayName(uint inputid)
{
    if (!inputid)
        return {};

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT displayname "
                  "FROM capturecard "
                  "WHERE cardid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetDisplayName(uint)", query);
    else if (query.next())
    {
        QString result = query.value(0).toString();
        return result;
    }

    return {};
}

bool CardUtil::IsUniqueDisplayName(const QString &name, uint exclude_inputid)
{
    if (name.isEmpty())
        return false;

    QString   matching;
    bool      two { false };
    qsizetype idx = name.indexOf('/');
    if (idx >= 0)
    {
        matching = name.right(name.size() - idx -1);
        two = false;
    }
    else
    {
        matching = name.right(2);
        two = true;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardid, displayname "
                  "FROM capturecard "
                  "WHERE parentid = 0 "
                  "      AND cardid <> :INPUTID ");
    query.bindValue(":INPUTID", exclude_inputid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::IsUniqueDisplayName()", query);
        return false;
    }

    while (query.next())
    {
        QString dn = query.value(1).toString();
        idx = dn.indexOf('/');
        if (!two && idx >= 0)
        {
            if (dn.right(dn.size() - idx - 1) == matching)
                return false;
        }
        else if (dn.right(2) == matching.right(2))
        {
            return false;
        }
    }

    return true;
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
    query.bindValueNoNull(":DISPLAYNAME", displayname);
    query.bindValue(":DISHNETEIT", dishnet_eit);
    query.bindValue(":RECPRIORITY", recpriority);
    query.bindValue(":QUICKTUNE", quicktune);
    query.bindValue(":SCHEDORDER", schedorder);
    query.bindValue(":LIVETVORDER", livetvorder);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::CreateCardInput()", query);
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
        MythDB::DBError("CardUtil::CreateNewInputGroup 0", query);
        return 0;
    }

    if (query.next())
        return query.value(0).toUInt();

    query.prepare("SELECT MAX(inputgroupid) FROM inputgroup");
    if (!query.exec())
    {
        MythDB::DBError("CardUtil::CreateNewInputGroup 1", query);
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
        MythDB::DBError("CardUtil::CreateNewInputGroup 2", query);
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
    if (type == "FREEBOX"   || type == "IMPORT"   ||
        type == "DEMO"      || type == "EXTERNAL" ||
        type == "HDHOMERUN" )
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
        return 0;
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
        MythDB::DBError("CardUtil::LinkInputGroup() 1", query);
        return false;
    }

    QString name;
    while (query.next()) {
        name = query.value(2).toString();
        uint cardid = query.value(0).toUInt();
        // Already linked
        if (cardid == inputid)
            return true;
    }

    // Invalid group id
    if (name.isEmpty())
        return false;

    query.prepare(
        "INSERT INTO inputgroup "
        "       (cardinputid, inputgroupid, inputgroupname) "
        "VALUES (:INPUTID,    :GROUPID,     :GROUPNAME    ) ");

    query.bindValue(":INPUTID",   inputid);
    query.bindValue(":GROUPID",   inputgroupid);
    query.bindValue(":GROUPNAME", name);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::LinkInputGroup() 2", query);
        return false;
    }

    // Now that there is a proper linkage, unlink temporary cardid 0
    UnlinkInputGroup(0, inputgroupid);

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

std::vector<uint> CardUtil::GetInputGroups(uint inputid)
{
    std::vector<uint> list;

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

std::vector<uint> CardUtil::GetGroupInputIDs(uint inputgroupid)
{
    std::vector<uint> list;

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

std::vector<uint> CardUtil::GetConflictingInputs(uint inputid)
{
    std::vector<uint> inputids;

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
        "                      AND c.sourceid > 0 "
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
    }

    if (VERBOSE_LEVEL_CHECK(VB_RECORD, LOG_INFO))
    {
        QString msg = QString("CardUtil[%1]: GetConflictingInputs(%1) ").arg(inputid);
        QStringList ids;
        for (auto id : inputids)
        {
            ids.append(QString::number(id));
        }
        msg.append(ids.join(','));
        LOG(VB_RECORD, LOG_INFO, msg);
   }
    return inputids;
}

bool CardUtil::GetTimeouts(uint inputid,
                           std::chrono::milliseconds &signal_timeout,
                           std::chrono::milliseconds &channel_timeout)
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
        signal_timeout  = std::max(std::chrono::milliseconds(query.value(0).toInt()), 250ms);
        channel_timeout = std::max(std::chrono::milliseconds(query.value(1).toInt()), 500ms);
        return true;
    }

    return false;
}

bool CardUtil::IsInNeedOfExternalInputConf(uint inputid)
{
    DiSEqCDevTree *diseqc_tree = DiSEqCDev::FindTree(inputid);

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

bool CardUtil::hasV4L2([[maybe_unused]] int videofd)
{
#ifdef USING_V4L2
    struct v4l2_capability vcap {};

    return ((ioctl(videofd, VIDIOC_QUERYCAP, &vcap) >= 0) &&
            ((vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0U));
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
    struct v4l2_capability capability {};
    if (ioctl(videofd, VIDIOC_QUERYCAP, &capability) >= 0)
    {
        input = QString::fromLatin1((const char*)capability.card);
        driver = QString::fromLatin1((const char*)capability.driver);
        version = capability.version;
        capabilities = capability.capabilities;
    }
#endif // USING_V4L2

    static const QRegularExpression kBracketedDigitRE { R"(\[[0-9]\]$)" };
    if (!driver.isEmpty())
        driver.remove( kBracketedDigitRE );

    return !input.isEmpty();
}

InputNames CardUtil::ProbeV4LVideoInputs([[maybe_unused]] int videofd, bool &ok)
{
    InputNames list;
    ok = false;

#ifdef USING_V4L2
    bool usingv4l2 = hasV4L2(videofd);

    struct v4l2_input vin {};
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

    // Create an input when none are advertised
    if (list.isEmpty())
        list[0] = "Television";

    ok = true;
#else // if !USING_V4L2
    list[-1] += QObject::tr("ERROR, Compile with V4L support to query inputs");
#endif // !USING_V4L2
    return list;
}

InputNames CardUtil::ProbeV4LAudioInputs([[maybe_unused]] int videofd, bool &ok)
{
    InputNames list;
    ok = false;

#ifdef USING_V4L2
    bool usingv4l2 = hasV4L2(videofd);

    // V4L v2 query
    struct v4l2_audio ain {};
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

// Convert DVBv5 frontend capabilities from bitmask to string
//
// Source linuxtv.org v4l-utils/lib/libdvbv5/dvb-v5.c
//
QStringList CardUtil::CapabilitiesToString([[maybe_unused]] uint64_t capabilities)
{
    QStringList caps;
#ifdef USING_DVB

    struct fe_caps_name {
        unsigned  idx;
        const char *name;
    };

    std::array<fe_caps_name,31> fe_caps_name {{
        { FE_CAN_2G_MODULATION,          "CAN_2G_MODULATION" },
        { FE_CAN_8VSB,                   "CAN_8VSB" },
        { FE_CAN_16VSB,                  "CAN_16VSB" },
        { FE_CAN_BANDWIDTH_AUTO,         "CAN_BANDWIDTH_AUTO" },
        { FE_CAN_FEC_1_2,                "CAN_FEC_1_2" },
        { FE_CAN_FEC_2_3,                "CAN_FEC_2_3" },
        { FE_CAN_FEC_3_4,                "CAN_FEC_3_4" },
        { FE_CAN_FEC_4_5,                "CAN_FEC_4_5" },
        { FE_CAN_FEC_5_6,                "CAN_FEC_5_6" },
        { FE_CAN_FEC_6_7,                "CAN_FEC_6_7" },
        { FE_CAN_FEC_7_8,                "CAN_FEC_7_8" },
        { FE_CAN_FEC_8_9,                "CAN_FEC_8_9" },
        { FE_CAN_FEC_AUTO,               "CAN_FEC_AUTO" },
        { FE_CAN_GUARD_INTERVAL_AUTO,    "CAN_GUARD_INTERVAL_AUTO" },
        { FE_CAN_HIERARCHY_AUTO,         "CAN_HIERARCHY_AUTO" },
        { FE_CAN_INVERSION_AUTO,         "CAN_INVERSION_AUTO" },
        { FE_CAN_MULTISTREAM,            "CAN_MULTISTREAM" },
        { FE_CAN_MUTE_TS,                "CAN_MUTE_TS" },
        { FE_CAN_QAM_16,                 "CAN_QAM_16" },
        { FE_CAN_QAM_32,                 "CAN_QAM_32" },
        { FE_CAN_QAM_64,                 "CAN_QAM_64" },
        { FE_CAN_QAM_128,                "CAN_QAM_128" },
        { FE_CAN_QAM_256,                "CAN_QAM_256" },
        { FE_CAN_QAM_AUTO,               "CAN_QAM_AUTO" },
        { FE_CAN_QPSK,                   "CAN_QPSK" },
        { FE_CAN_RECOVER,                "CAN_RECOVER" },
        { FE_CAN_TRANSMISSION_MODE_AUTO, "CAN_TRANSMISSION_MODE_AUTO" },
        { FE_CAN_TURBO_FEC,              "CAN_TURBO_FEC" },
        { FE_HAS_EXTENDED_CAPS,          "HAS_EXTENDED_CAPS" },
        { FE_IS_STUPID,                  "IS_STUPID" },
        { FE_NEEDS_BENDING,              "NEEDS_BENDING" },
    }};

    for (const auto & cap : fe_caps_name)
    {
        if (capabilities & cap.idx)
            caps.append(cap.name);
    }
#endif  // USING_DVB
    return caps;
}

QStringList CardUtil::ProbeVideoInputs(const QString& device, const QString& inputtype)
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

QStringList CardUtil::ProbeAudioInputs(const QString& device, const QString& inputtype)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("ProbeAudioInputs(%1,%2)")
                                   .arg(device, inputtype));
    QStringList ret;

    if ("HDPVR" == inputtype ||
        "V4L2"  == inputtype)
        ret += ProbeV4LAudioInputs(device);

    return ret;
}

QStringList CardUtil::ProbeV4LVideoInputs(const QString& device)
{
    bool ok = false;
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

QStringList CardUtil::ProbeV4LAudioInputs(const QString& device)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("ProbeV4LAudioInputs(%1)").arg(device));

    bool ok = false;
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

QStringList CardUtil::ProbeDVBInputs([[maybe_unused]] const QString& device)
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
    ret += QObject::tr("ERROR, Compile with DVB support to query inputs");
#endif

    return ret;
}

QString CardUtil::GetDeviceLabel(const QString &inputtype,
                                 const QString &videodevice)
{
    return QString("[ %1 : %2 ]").arg(inputtype, videodevice);
}

QString CardUtil::GetDeviceLabel(uint inputid)
{
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
                                 const std::chrono::milliseconds signal_timeout,
                                 const std::chrono::milliseconds channel_timeout,
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
    if (audiodevice.length() == 0) // Empty string is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":AUDIODEVICE", QVariant(QVariant::String));
#else
        query.bindValue(":AUDIODEVICE", QVariant(QMetaType(QMetaType::QString)));
#endif
    }
    else
    {
        query.bindValue(":AUDIODEVICE", audiodevice);
    }
    if (vbidevice.length() == 0) // Empty string is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":VBIDEVICE", QVariant(QVariant::String));
#else
        query.bindValue(":VBIDEVICE", QVariant(QMetaType(QMetaType::QString)));
#endif
    }
    else
    {
        query.bindValue(":VBIDEVICE", vbidevice);
    }
    query.bindValue(":INPUTTYPE", inputtype);
    if (audioratelimit == 0) // Value 0 is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":AUDIORATELIMIT", QVariant(QVariant::UInt));
#else
        query.bindValue(":AUDIORATELIMIT", QVariant(QMetaType(QMetaType::UInt)));
#endif
    }
    else
    {
        query.bindValue(":AUDIORATELIMIT", audioratelimit);
    }
    query.bindValue(":HOSTNAME", hostname);
    query.bindValue(":DVBSWFILTER", dvb_swfilter);
    query.bindValue(":DVBSATTYPE", dvb_sat_type);
    query.bindValue(":DVBWAITFORSEQSTART", dvb_wait_for_seqstart);
    query.bindValue(":SKIPBTAUDIO", skipbtaudio);
    query.bindValue(":DVBONDEMAND", dvb_on_demand);
    query.bindValue(":DVBDISEQCTYPE", dvb_diseqc_type);
    query.bindValue(":FIREWIRESPEED", firewire_speed);
    if (firewire_model.length() == 0) // Empty string is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":FIREWIREMODEL", QVariant(QVariant::String));
#else
        query.bindValue(":FIREWIREMODEL", QVariant(QMetaType(QMetaType::QString)));
#endif
    }
    else
    {
        query.bindValue(":FIREWIREMODEL", firewire_model);
    }
    query.bindValue(":FIREWIRECONNECTION", firewire_connection);
    query.bindValue(":SIGNALTIMEOUT", static_cast<qint64>(signal_timeout.count()));
    query.bindValue(":CHANNELTIMEOUT", static_cast<qint64>(channel_timeout.count()));
    query.bindValue(":DVBTUNINGDELAY", dvb_tuning_delay);
    query.bindValue(":CONTRAST", contrast);
    query.bindValue(":BRIGHTNESS", brightness);
    query.bindValue(":COLOUR", colour);
    query.bindValue(":HUE", hue);
    if (diseqcid == 0) // Value 0 is set to null
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        query.bindValue(":DISEQCID", QVariant(QVariant::UInt));
#else
        query.bindValue(":DISEQCID", QVariant(QMetaType(QMetaType::UInt)));
#endif
    }
    else
    {
        query.bindValue(":DISEQCID", diseqcid);
    }
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
    std::vector<uint> childids = GetChildInputIDs(inputid);
    for (uint childid : childids)
    {
        if (!DeleteInput(childid))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("CardUtil: Failed to delete child input %1")
                .arg(childid));
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
            tree.SetRoot(nullptr);
            tree.Store(inputid);
        }
    }

    // Delete any unused input groups
    UnlinkInputGroup(0, 0);

    return true;
}

bool CardUtil::DeleteAllInputs(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    return (query.exec("TRUNCATE TABLE inputgroup") &&
            query.exec("TRUNCATE TABLE diseqc_config") &&
            query.exec("TRUNCATE TABLE diseqc_tree") &&
            query.exec("TRUNCATE TABLE capturecard"));
}

std::vector<uint> CardUtil::GetInputList(void)
{
    std::vector<uint> list;

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

std::vector<uint> CardUtil::GetSchedInputList(void)
{
    std::vector<uint> list;

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

std::vector<uint> CardUtil::GetLiveTVInputList(void)
{
    std::vector<uint> list;

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
#if 0
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("DVB Device (%1)").arg(devname));
#endif

    if (DVB_DEV_FRONTEND == type)
    {
        return devname;
    }

    if (DVB_DEV_DVR == type)
    {
        QString tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 8, "dvr");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend dvr number matches (%1)").arg(tmp));
            return tmp;
        }

        // Use dvr0, allows multi-standard frontends which only have one dvr
        tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 9, "dvr0");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend dvr number not matching, using dvr0 instead (%1)").arg(tmp));
            return tmp;
        }

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Adapter Frontend no dvr device found for (%1)").arg(devname));
        return "";
    }

    if (DVB_DEV_DEMUX == type)
    {
        QString tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 8, "demux");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend demux number matches (%1)").arg(tmp));
            return tmp;
        }

        // Use demux0, allows multi-standard frontends, which only have one demux
        tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 9, "demux0");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend demux number not matching, using demux0 instead (%1)").arg(tmp));
            return tmp;
        }

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Adapter Frontend no demux device found for (%1)").arg(devname));
        return "";
    }

    if (DVB_DEV_CA == type)
    {
        QString tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 8, "ca");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend ca number matches (%1)").arg(tmp));
            return tmp;
        }

        // Use ca0, allows multi-standard frontends, which only have one ca
        tmp = devname;
        tmp = tmp.replace(tmp.indexOf("frontend"), 9, "ca0");
        if (QFile::exists(tmp))
        {
            LOG(VB_RECORD, LOG_DEBUG, LOC +
                QString("Adapter Frontend ca number not matching, using ca0 instead (%1)").arg(tmp));
            return tmp;
        }

        LOG(VB_RECORD, LOG_DEBUG, LOC +
            QString("Adapter Frontend no ca device found for (%1)").arg(devname));
        return "";
    }

    if (DVB_DEV_AUDIO == type)
    {
        return devname.replace(devname.indexOf("frontend"), 8, "audio");
    }

    if (DVB_DEV_VIDEO == type)
    {
        return devname.replace(devname.indexOf("frontend"), 8, "video");
    }

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

bool CardUtil::HDHRdoesDVB([[maybe_unused]] const QString &device)
{
#ifdef USING_HDHOMERUN
    hdhomerun_device_t *hdhr =
        hdhomerun_device_create_from_str(device.toLatin1(), nullptr);
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
 * If the device is valid, check if the model does DVB-C.
 */

bool CardUtil::HDHRdoesDVBC([[maybe_unused]] const QString &device)
{
#ifdef USING_HDHOMERUN
    hdhomerun_device_t *hdhr =
        hdhomerun_device_create_from_str(device.toLatin1(), nullptr);
    if (!hdhr)
        return false;

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (model && strstr(model, "dvbc"))
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

QString CardUtil::GetHDHRdesc([[maybe_unused]] const QString &device)
{
    QString connectErr = QObject::tr("Unable to connect to device.");

#ifdef USING_HDHOMERUN
    [[maybe_unused]] bool deviceIsIP = false;

    if (device.contains('.'))  // Simplistic check, but also allows DNS names
        deviceIsIP = true;
    else
    {
        bool validID = false;

        uint32_t dev = device.toUInt(&validID, 16);
        if (!validID || !hdhomerun_discover_validate_device_id(dev))
            return QObject::tr("Invalid Device ID");
    }

    LOG(VB_GENERAL, LOG_INFO, "CardUtil::GetHDHRdescription(" + device +
                              ") - trying to locate device");

    hdhomerun_device_t *hdhr =
        hdhomerun_device_create_from_str(device.toLatin1(), nullptr);
    if (!hdhr)
        return QObject::tr("Invalid Device ID or address.");

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (!model)
    {
        hdhomerun_device_destroy(hdhr);
        return connectErr;
    }


    QString   description = model;
    char     *sVersion = nullptr;
    uint32_t  iVersion = 0;

    if (hdhomerun_device_get_version(hdhr, &sVersion, &iVersion))
        description += QObject::tr(", firmware: %2").arg(sVersion);

    hdhomerun_device_destroy(hdhr);

    return description;
#else
    return connectErr;
#endif
}

/**
 * Get a nicely formatted string describing the device
 */

QString CardUtil::GetVBoxdesc([[maybe_unused]] const QString &id,
                              [[maybe_unused]] const QString &ip,
                              [[maybe_unused]] const QString &tunerNo,
                              [[maybe_unused]] const QString &tunerType)
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
                                            .arg(version, VBOX_MIN_API_VERSION);
        delete vbox;
        return apiVersionErr;

    }

    delete vbox;

    return QString("V@Box TV Gateway - ID: %1, IP: %2, Tuner: %3-%4")
                   .arg(id, ip, tunerNo, tunerType);

#else
    return connectErr;
#endif
}

#ifdef USING_ASI
static QString sys_dev(uint device_num, const QString& dev)
{
    return QString("/sys/class/asi/asirx%1/%2").arg(device_num).arg(dev);
}

static QString read_sys(const QString& sys_dev)
{
    QFile f(sys_dev);
    f.open(QIODevice::ReadOnly);
    QByteArray sdba = f.readAll();
    f.close();
    return sdba;
}

static bool write_sys(const QString& sys_dev, const QString& str)
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

int CardUtil::GetASIDeviceNumber([[maybe_unused]] const QString &device, QString *error)
{
#ifdef USING_ASI
    // basic confirmation
    struct stat statbuf {};
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
    if (error)
        *error = "Not compiled with ASI support.";
    return -1;
#endif
}

uint CardUtil::GetASIBufferSize([[maybe_unused]] uint device_num, QString *error)
{
#ifdef USING_ASI
    // get the buffer size
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "bufsize"));
    bool ok = false;
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
    if (error)
        *error = "Not compiled with ASI support.";
    return 0;
#endif
}

uint CardUtil::GetASINumBuffers([[maybe_unused]] uint device_num, QString *error)
{
#ifdef USING_ASI
    // get the buffer size
    QString sys_numbuffers_contents = read_sys(sys_dev(device_num, "buffers"));
    bool ok = false;
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
    if (error)
        *error = "Not compiled with ASI support.";
    return 0;
#endif
}

int CardUtil::GetASIMode([[maybe_unused]] uint device_num, QString *error)
{
#ifdef USING_ASI
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "mode"));
    bool ok = false;
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
    if (error)
        *error = "Not compiled with ASI support.";
    return -1;
#endif
}

bool CardUtil::SetASIMode([[maybe_unused]] uint device_num,
                          [[maybe_unused]] uint mode,
                          QString *error)
{
#ifdef USING_ASI
    QString sys_bufsize_contents = read_sys(sys_dev(device_num, "mode"));
    bool ok = false;
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
    if (error)
        *error = "Not compiled with ASI support.";
    return false;
#endif
}

#ifdef USING_VBOX
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
    chanid = ChannelUtil::GetChannelValueInt("chanid",GetSourceID(inputid),GetStartChannel(inputid));
    if (!chanid)
    {
        // no chanid, presume bad setup
        LOG(VB_GENERAL, LOG_ERR, QString("VBOX chanid (%1) not found for inputid (%2), redo mythtv-setup")
                .arg(chanid).arg(inputid));
        return false;
    }

    // get timeouts for inputid
    std::chrono::milliseconds signal_timeout = 0ms;
    std::chrono::milliseconds tuning_timeout = 0ms;
    if (!GetTimeouts(inputid,signal_timeout,tuning_timeout))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to get timeouts for inputid (%1)")
                .arg(inputid));
        return false;
    }

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

    // now get just the IP address from the url
    QString ip = url.host();
    LOG(VB_GENERAL, LOG_INFO, QString("VBOX IP found (%1) for inputid (%2)")
                .arg(ip).arg(inputid));

    if (!ping(ip,signal_timeout))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("VBOX at IP (%1) failed to respond to network ping for inputid (%2) timeout (%3)")
            .arg(ip).arg(inputid).arg(signal_timeout.count()));
        return false;
    }

    return true;
}
#endif

#ifdef USING_SATIP
/** \fn CardUtil::IsSatIPPresent(uint inputid)
 *  \brief Returns true if the SatIP box responds to a ping
 *  \param inputid  As used in DB capturecard table field cardid
 */
bool CardUtil::IsSatIPPresent(uint inputid)
{
    // Should only be called if inputtype == SATIP
    if (!inputid )
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SatIP inputid (%1) not valid, redo mythtv-setup")
                .arg(inputid));
        return false;
    }

    // Get sourceid and startchan from table capturecard for inputid
    uint chanid = 0;
    chanid = ChannelUtil::GetChannelValueInt("chanid", GetSourceID(inputid), GetStartChannel(inputid));
    if (!chanid)
    {
        // no chanid, presume bad setup
        LOG(VB_GENERAL, LOG_ERR, QString("SatIP chanid (%1) not found for inputid (%2), redo mythtv-setup")
                .arg(chanid).arg(inputid));
        return false;
    }

    // Get timeouts for inputid
    std::chrono::milliseconds signal_timeout = 0ms;
    std::chrono::milliseconds tuning_timeout = 0ms;
    if (!GetTimeouts(inputid,signal_timeout,tuning_timeout))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to get timeouts for inputid (%1)")
                .arg(inputid));
        return false;
    }

    // Ping the SatIP box to see if it can be reached on the network
    QString device = CardUtil::GetVideoDevice(inputid);
    QStringList devinfo = device.split(":");
    if (devinfo.value(0).toUpper() == "UUID")
    {
        QString deviceId = QString("uuid:%1").arg(devinfo.value(1));
        QString ip = SatIP::findDeviceIP(deviceId);
        LOG(VB_GENERAL, LOG_INFO, QString("SatIP[%1] IP address %2 device %3")
                    .arg(inputid).arg(ip, device));

        if (!ping(ip, signal_timeout))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("SatIP[%1] at IP %2 failed to respond to network ping (timeout %3)")
                .arg(inputid).arg(ip).arg(signal_timeout.count()));
            return false;
        }
        return true;
    }
    return false;
}
#endif

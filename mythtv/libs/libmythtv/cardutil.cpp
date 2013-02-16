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

#ifdef USING_DVB
#include "dvbtypes.h"
#endif

#ifdef USING_V4L1
#include <linux/videodev.h>
#endif

#ifdef USING_V4L2
#include <linux/videodev2.h>
#endif

#ifdef USING_HDHOMERUN
#include "hdhomerun.h"
#endif

#ifdef USING_ASI
#include <dveo/asi.h>
#include <dveo/master.h>
#endif

#define LOC      QString("CardUtil: ")

QString CardUtil::GetScanableCardTypes(void)
{
    QString cardTypes = "";

#ifdef USING_DVB
    cardTypes += "'DVB'";
#endif // USING_DVB

#ifdef USING_V4L2
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'V4L'";
# ifdef USING_IVTV
    cardTypes += ",'MPEG'";
# endif // USING_IVTV
#endif // USING_V4L2

#ifdef USING_IPTV
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'FREEBOX'";
#endif // USING_IPTV

#ifdef USING_HDHOMERUN
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'HDHOMERUN'";
#endif // USING_HDHOMERUN

#ifdef USING_ASI
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'ASI'";
#endif

#ifdef USING_CETON
    if (!cardTypes.isEmpty())
        cardTypes += ",";
    cardTypes += "'CETON'";
#endif // USING_CETON

    if (cardTypes.isEmpty())
        cardTypes = "'DUMMY'";

    return QString("(%1)").arg(cardTypes);
}

bool CardUtil::IsCableCardPresent(uint cardid,
                                  const QString &cardType)
{
    if (cardType == "HDHOMERUN")
    {
#ifdef USING_HDHOMERUN
        hdhomerun_device_t *hdhr;
        hdhomerun_tuner_status_t status;
        QString device = GetVideoDevice(cardid);
        hdhr = hdhomerun_device_create_from_str(device.toAscii(), NULL);
        if (!hdhr)
            return false;

        int oob = -1;
        oob = hdhomerun_device_get_oob_status(hdhr, NULL, &status);

        // if no OOB tuner, oob will be < 1.  If no CC present, OOB
        // status will be "none."
        if (oob > 0 && (strncmp(status.channel, "none", 4) != 0))
        {
            LOG(VB_GENERAL, LOG_INFO, "Cardutil: HDHomeRun Cablecard Present.");
            return true;
        }
        else
#endif
            return false;
    }
    else if (cardType == "CETON")
    {
#ifdef USING_CETON
        // TODO FIXME implement detection of Cablecard presence
        LOG(VB_GENERAL, LOG_INFO, "Cardutil: TODO Ceton Is Cablecard Present?");
        return true;
#else
        return false;
#endif
    }
    else
        return false;
}

bool CardUtil::IsTunerShared(uint cardidA, uint cardidB)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("IsTunerShared(%1,%2)")
            .arg(cardidA).arg(cardidB));

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, hostname, cardtype "
                  "FROM capturecard "
                  "WHERE ( (cardid = :CARDID_A) OR "
                  "        (cardid = :CARDID_B) )");
    query.bindValue(":CARDID_A", cardidA);
    query.bindValue(":CARDID_B", cardidB);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::is_tuner_shared", query);
        return false;
    }

    if (!query.next())
        return false;

    const QString vdevice  = query.value(0).toString();
    const QString hostname = query.value(1).toString();
    const QString cardtype = query.value(2).toString();

    if (!IsTunerSharingCapable(cardtype.toUpper()))
        return false;

    if (!query.next())
        return false;

    bool ret = ((vdevice  == query.value(0).toString()) &&
                (hostname == query.value(1).toString()) &&
                (cardtype == query.value(2).toString()));

    LOG(VB_RECORD, LOG_DEBUG, QString("IsTunerShared(%1,%2) -> %3")
            .arg(cardidA).arg(cardidB).arg(ret));

    return ret;
}

/** \fn CardUtil::IsCardTypePresent(const QString&, QString)
 *  \brief Returns true if the card type is present and connected to an input
 *  \param rawtype  Card type as used in DB or empty string for all cards
 *  \param hostname Host to check, or empty string for current host
 */
bool CardUtil::IsCardTypePresent(const QString &rawtype, QString hostname)
{
    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        "SELECT count(cardtype) "
        "FROM capturecard, cardinput "
        "WHERE cardinput.cardid = capturecard.cardid AND "
        "      capturecard.hostname = :HOSTNAME";

    if (!rawtype.isEmpty())
        qstr += " AND capturecard.cardtype = :CARDTYPE";

    query.prepare(qstr);

    if (!rawtype.isEmpty())
        query.bindValue(":CARDTYPE", rawtype.toUpper());

    query.bindValue(":HOSTNAME", hostname);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::IsCardTypePresent", query);
        return false;
    }

    uint count = 0;
    if (query.next())
        count = query.value(0).toUInt();

    return count > 0;
}

QStringList CardUtil::GetCardTypes(void)
{
    QStringList cardtypes;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT cardtype "
                  "FROM capturecard");

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetCardTypes()", query);
    }
    else
    {
        while (query.next())
            cardtypes.push_back(query.value(0).toString());
    }

    return cardtypes;
}

/** \fn CardUtil::GetVideoDevices(const QString&, QString)
 *  \brief Returns the videodevices of the matching cards, duplicates removed
 *  \param rawtype  Card type as used in DB or empty string for all cardids
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
        qstr += " AND cardtype = :CARDTYPE";

    query.prepare(qstr);

    if (!rawtype.isEmpty())
        query.bindValue(":CARDTYPE", rawtype.toUpper());

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

QStringList CardUtil::ProbeVideoDevices(const QString &rawtype)
{
    QStringList devs;

    if (rawtype.toUpper() == "DVB")
    {
        QDir dir("/dev/dvb", "adapter*", QDir::Name, QDir::Dirs);
        const QFileInfoList il = dir.entryInfoList();
        if (il.isEmpty())
            return devs;

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
        if (il.isEmpty())
            return devs;

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
            return devs;
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

    return devs;
}

QString CardUtil::ProbeDVBType(const QString &device)
{
    QString ret = "ERROR_UNKNOWN";

    if (device.isEmpty())
        return ret;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toAscii();
    int fd_frontend = open(dev.constData(), O_RDONLY | O_NONBLOCK);
    if (fd_frontend < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Can't open DVB frontend (%1) for %2.")
                .arg(dvbdev).arg(device));
        return ret;
    }

    struct dvb_frontend_info info;
    int err = ioctl(fd_frontend, FE_GET_INFO, &info);
    if (err < 0)
    {
        close(fd_frontend);
        LOG(VB_GENERAL, LOG_ERR, QString("FE_GET_INFO ioctl failed (%1)")
                                         .arg(dvbdev) + ENO);
        return ret;
    }
    close(fd_frontend);

    DTVTunerType type(info.type);
#if HAVE_FE_CAN_2G_MODULATION
    if (type == DTVTunerType::kTunerTypeDVBS1 &&
        (info.caps & FE_CAN_2G_MODULATION))
        type = DTVTunerType::kTunerTypeDVBS2;
#endif // HAVE_FE_CAN_2G_MODULATION
    ret = (type.toString() != "UNKNOWN") ? type.toString().toUpper() : ret;
#endif // USING_DVB

    return ret;
}

/** \fn CardUtil::ProbeDVBFrontendName(const QString &)
 *  \brief Returns the card type from the video device
 */
QString CardUtil::ProbeDVBFrontendName(const QString &device)
{
    QString ret = "ERROR_UNKNOWN";
    (void) device;

#ifdef USING_DVB
    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_FRONTEND, device);
    QByteArray dev = dvbdev.toAscii();
    int fd_frontend = open(dev.constData(), O_RDWR | O_NONBLOCK);
    if (fd_frontend < 0)
        return "ERROR_OPEN";

    struct dvb_frontend_info info;
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
 *        firmware (See http://svn.mythtv.org/trac/ticket/3541).
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

QString CardUtil::ProbeSubTypeName(uint cardid)
{
    QString type = GetRawCardType(cardid);
    if ("DVB" != type)
        return type;

    QString device = GetVideoDevice(cardid);

    if (device.isEmpty())
        return "ERROR_OPEN";

    return ProbeDVBType(device);
}

/// \brief Returns true iff the card_type is one of the DVB types.
bool CardUtil::IsDVBCardType(const QString &card_type)
{
    QString ct = card_type.toUpper();
    return (ct == "DVB") || (ct == "QAM") || (ct == "QPSK") ||
        (ct == "OFDM") || (ct == "ATSC") || (ct == "DVB_S2");
}

QString get_on_cardid(const QString &to_get, uint cardid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("SELECT %1 ").arg(to_get) +
        "FROM capturecard "
        "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec())
        MythDB::DBError("CardUtil::get_on_source", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

bool set_on_source(const QString &to_set, uint cardid, uint sourceid,
                   const QString value)
{
    QString tmp = get_on_cardid("capturecard.cardid", cardid);
    if (tmp.isEmpty())
        return false;

    bool ok;
    uint input_cardid = tmp.toUInt(&ok);
    if (!ok)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("UPDATE capturecard SET %1 = :VALUE ").arg(to_set) +
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", input_cardid);
    query.bindValue(":VALUE",  value);

    if (query.exec())
        return true;

    MythDB::DBError("CardUtil::set_on_source", query);
    return false;
}

QString get_on_inputid(const QString &to_get, uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("SELECT %1 ").arg(to_get) +
        "FROM cardinput "
        "WHERE cardinput.cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::get_on_inputid", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

bool set_on_input(const QString &to_set, uint inputid, const QString value)
{
    QString tmp = get_on_inputid("cardinput.cardinputid", inputid);
    if (tmp.isEmpty())
        return false;

    bool ok;
    uint input_cardinputid = tmp.toUInt(&ok);
    if (!ok)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("UPDATE cardinput SET %1 = :VALUE ").arg(to_set) +
        "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", input_cardinputid);
    query.bindValue(":VALUE",  value);

    if (query.exec())
        return true;

    MythDB::DBError("CardUtil::set_on_input", query);
    return false;
}

/**
 *  \brief Returns all cardids of cards that uses the specified
 *         videodevice if specified, and optionally rawtype and a non-local
 *         hostname. The result is ordered from smallest to largest.
 *  \param videodevice Video device we want card ids for
 *  \param rawtype     Card type as used in DB or empty string for any type
 *  \param hostname    Host on which device resides, only
 *                     required if said host is not the localhost
 */
vector<uint> CardUtil::GetCardIDs(QString videodevice,
                                  QString rawtype,
                                  QString hostname)
{
    vector<uint> list;

    if (hostname.isEmpty())
        hostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString qstr =
        (videodevice.isEmpty()) ?
        "SELECT cardid "
        "FROM capturecard "
        "WHERE hostname    = :HOSTNAME" :

        "SELECT cardid "
        "FROM capturecard "
        "WHERE videodevice = :DEVICE AND "
        "      hostname    = :HOSTNAME";

    if (!rawtype.isEmpty())
        qstr += " AND cardtype = :CARDTYPE";

    qstr += " ORDER BY cardid";

    query.prepare(qstr);

    if (!videodevice.isEmpty())
        query.bindValue(":DEVICE",   videodevice);

    query.bindValue(":HOSTNAME", hostname);

    if (!rawtype.isEmpty())
        query.bindValue(":CARDTYPE", rawtype.toUpper());

    if (!query.exec())
        MythDB::DBError("CardUtil::GetCardIDs(videodevice...)", query);
    else
    {
        while (query.next())
            list.push_back(query.value(0).toUInt());
    }

    return list;
}

static uint clone_capturecard(uint src_cardid, uint orig_dst_cardid)
{
    uint dst_cardid = orig_dst_cardid;

    MSqlQuery query(MSqlQuery::InitCon());
    if (!dst_cardid)
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

        dst_cardid = query.value(0).toUInt();
    }

    query.prepare(
        "SELECT videodevice,           cardtype,                         "
        "       hostname,              signal_timeout, channel_timeout,  "
        "       dvb_wait_for_seqstart, dvb_on_demand,  dvb_tuning_delay, "
        "       dvb_diseqc_type,       diseqcid,       dvb_eitscan "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", src_cardid);

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

    MSqlQuery query2(MSqlQuery::InitCon());
    query2.prepare(
        "UPDATE capturecard "
        "SET videodevice           = :V0, "
        "    cardtype              = :V1, "
        "    hostname              = :V2, "
        "    signal_timeout        = :V3, "
        "    channel_timeout       = :V4, "
        "    dvb_wait_for_seqstart = :V5, "
        "    dvb_on_demand         = :V6, "
        "    dvb_tuning_delay      = :V7, "
        "    dvb_diseqc_type       = :V8, "
        "    diseqcid              = :V9,"
        "    dvb_eitscan           = :V10 "
        "WHERE cardid = :CARDID");
    for (uint i = 0; i < 11; i++)
        query2.bindValue(QString(":V%1").arg(i), query.value(i).toString());
    query2.bindValue(":CARDID", dst_cardid);

    if (!query2.exec())
    {
        MythDB::DBError("clone_capturecard -- save data", query2);
        if (!orig_dst_cardid)
            CardUtil::DeleteCard(dst_cardid);
        return 0;
    }

    return dst_cardid;
}

static bool clone_cardinputs(uint src_cardid, uint dst_cardid)
{
    vector<uint> src_inputs = CardUtil::GetInputIDs(src_cardid);
    vector<uint> dst_inputs = CardUtil::GetInputIDs(dst_cardid);
    vector<QString> src_names;
    vector<QString> dst_names;
    QMap<uint,bool> dst_keep;

    for (uint i = 0; i < src_inputs.size(); i++)
        src_names.push_back(CardUtil::GetInputName(src_inputs[i]));

    for (uint i = 0; i < dst_inputs.size(); i++)
        dst_names.push_back(CardUtil::GetInputName(dst_inputs[i]));

    bool ok = true;

    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    for (uint i = 0; i < src_inputs.size(); i++)
    {
        query.prepare(
            "SELECT sourceid,        inputname,       externalcommand, "
            "       tunechan,        startchan,       displayname,     "
            "       dishnet_eit,     recpriority,     quicktune,       "
            "       schedorder,      livetvorder                       "
            "FROM cardinput "
            "WHERE cardinputid = :INPUTID");
        query.bindValue(":INPUTID", src_inputs[i]);
        if (!query.exec())
        {
            MythDB::DBError("clone_cardinput -- get data", query);
            ok = false;
            break;
        }
        if (!query.next())
        {
            LOG(VB_GENERAL, LOG_ERR, "clone_cardinput -- get data 2");
            ok = false;
            break;
        }

        int match = -1;
        for (uint j = 0; j < dst_inputs.size(); j++)
        {
            if (src_names[i] == dst_names[j])
            {
                match = (int) j;
                break;
            }
        }

        uint dst_inputid = 0;
        if (match >= 0)
        {
            dst_keep[match] = true;

            // copy data from src[i] to dst[match]
            query2.prepare(
                "UPDATE cardinput "
                "SET sourceid        = :V0, "
                "    inputname       = :V1, "
                "    externalcommand = :V2, "
                "    tunechan        = :V3, "
                "    startchan       = :V4, "
                "    displayname     = :V5, "
                "    dishnet_eit     = :V6, "
                "    recpriority     = :V7, "
                "    quicktune       = :V8, "
                "    schedorder      = :V9, "
                "    livetvorder     = :V10 "
                "WHERE cardinputid = :INPUTID");

            for (uint j = 0; j < 11; j++)
            {
                query2.bindValue(QString(":V%1").arg(j),
                                 query.value(j).toString());
            }
            query2.bindValue(":INPUTID", dst_inputs[match]);

            if (!query2.exec())
            {
                MythDB::DBError("clone_cardinput -- update data", query2);
                ok = false;
                break;
            }

            dst_inputid = dst_inputs[match];
        }
        else
        {
            // create new input for dst with data from src

            query2.prepare(
                "INSERT cardinput "
                "SET cardid          = :CARDID, "
                "    sourceid        = :V0, "
                "    inputname       = :V1, "
                "    externalcommand = :V2, "
                "    tunechan        = :V3, "
                "    startchan       = :V4, "
                "    displayname     = :V5, "
                "    dishnet_eit     = :V6, "
                "    recpriority     = :V7, "
                "    quicktune       = :V8, "
                "    schedorder      = :V9, "
                "    livetvorder     = :V10 ");

            query2.bindValue(":CARDID", dst_cardid);
            for (uint j = 0; j < 11; j++)
            {
                query2.bindValue(QString(":V%1").arg(j),
                                 query.value(j).toString());
            }

            if (!query2.exec())
            {
                MythDB::DBError("clone_cardinput -- insert data", query2);
                ok = false;
                break;
            }

            query2.prepare(
                "SELECT cardinputid "
                "FROM cardinput "
                "WHERE cardid    = :CARDID AND "
                "      inputname = :NAME");
            query2.bindValue(":CARDID", dst_cardid);
            query2.bindValue(":NAME", query.value(1).toString());
            if (!query2.exec())
            {
                MythDB::DBError("clone_cardinput -- "
                                     "insert, query inputid", query2);
                ok = false;
                break;
            }
            if (!query2.next())
            {
                LOG(VB_GENERAL, LOG_ERR, "clone_cardinput -- insert failed");
                ok = false;
                break;
            }

            dst_inputid = query2.value(0).toUInt();
        }

        // copy input group linkages
        vector<uint> src_grps = CardUtil::GetInputGroups(src_inputs[i]);
        vector<uint> dst_grps = CardUtil::GetInputGroups(dst_inputid);
        for (uint j = 0; j < dst_grps.size(); j++)
            CardUtil::UnlinkInputGroup(dst_inputid, dst_grps[j]);
        for (uint j = 0; j < src_grps.size(); j++)
            CardUtil::LinkInputGroup(dst_inputid, src_grps[j]);

        // clone diseqc_config (just points to the same diseqc_tree row)
        DiSEqCDevSettings diseqc;
        if (diseqc.Load(src_inputs[i]))
            diseqc.Store(dst_inputid);
    }

    // delete extra inputs in dst
    for (uint i = 0; i < dst_inputs.size(); i++)
    {
        if (!dst_keep[i])
            ok &= CardUtil::DeleteInput(dst_inputs[i]);
    }

    return ok;
}

bool CardUtil::CloneCard(uint src_cardid, uint orig_dst_cardid)
{
    QString type = CardUtil::GetRawCardType(src_cardid);
    if (!IsTunerSharingCapable(type))
        return false;

    uint dst_cardid = clone_capturecard(src_cardid, orig_dst_cardid);
    if (!dst_cardid)
        return false;

    if (!clone_cardinputs(src_cardid, dst_cardid) && !orig_dst_cardid)
    {
        DeleteCard(dst_cardid);
        return false;
    }

    return true;
}

vector<uint> CardUtil::GetCloneCardIDs(uint cardid)
{
    vector<uint> list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardtype, videodevice, hostname "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID",   cardid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetCloneCardIDs() 1", query);
        return list;
    }

    if (!query.next())
        return list;

    QString rawtype     = query.value(0).toString();
    QString videodevice = query.value(1).toString();
    QString hostname    = query.value(2).toString();

    if (!IsTunerSharingCapable(rawtype))
        return list;

    query.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "WHERE cardid      != :CARDID AND "
        "      videodevice  = :DEVICE AND "
        "      cardtype     = :TYPE   AND "
        "      hostname     = :HOSTNAME");
    query.bindValue(":CARDID",   cardid);
    query.bindValue(":DEVICE",   videodevice);
    query.bindValue(":TYPE",     rawtype);
    query.bindValue(":HOSTNAME", hostname);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetCloneCardIDs() 2", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

QString CardUtil::GetFirewireChangerNode(uint inputid)
{
    QString fwnode;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT changer_device "
                  "FROM cardinput WHERE cardinputid = :INPUTID ");
    query.bindValue(":CARDID", inputid);

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
                  "FROM cardinput WHERE cardinputid = :INPUTID ");
    query.bindValue(":CARDID", inputid);

    if (query.exec() && query.next())
    {
        fwnode = query.value(0).toString();
    }

    return fwnode;
}

vector<uint> CardUtil::GetCardIDs(uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM cardinput "
        "WHERE sourceid = :SOURCEID");
    query.bindValue(":SOURCEID", sourceid);

    vector<uint> list;

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetCardIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

int CardUtil::GetCardInputID(
    uint cardid, const QString &channum, QString &inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid, inputname "
        "FROM channel, capturecard, cardinput "
        "WHERE channel.channum      = :CHANNUM           AND "
        "      channel.sourceid     = cardinput.sourceid AND "
        "      cardinput.cardid     = capturecard.cardid AND "
        "      capturecard.cardid   = :CARDID");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("get_cardinputid", query);
    else if (query.next())
    {
        inputname = query.value(1).toString();
        return query.value(0).toInt();
    }

    return -1;
}

bool CardUtil::SetStartChannel(uint cardinputid, const QString &channum)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE cardinput "
                  "SET startchan = :CHANNUM "
                  "WHERE cardinputid = :INPUTID");
    query.bindValue(":CHANNUM", channum);
    query.bindValue(":INPUTID", cardinputid);

    if (!query.exec())
    {
        MythDB::DBError("set_startchan", query);
        return false;
    }

    return true;
}

/** \fn CardUtil::GetStartInput(uint)
 *  \brief Returns the start input for the card
 *  \param nCardID card id to check
 *  \return the start input
 */
QString CardUtil::GetStartInput(uint nCardID)
{
    QString str = QString::null;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputname "
                  "FROM cardinput "
                  "WHERE cardinput.cardid = :CARDID " 
                  "ORDER BY livetvorder = 0, livetvorder, cardinputid "
                  "LIMIT 1");
    query.bindValue(":CARDID", nCardID);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetStartInput()", query);
    else if (query.next())
        str = query.value(0).toString();

    return str;
}

QStringList CardUtil::GetInputNames(uint cardid, uint sourceid)
{
    QStringList list;
    MSqlQuery query(MSqlQuery::InitCon());

    if (sourceid)
    {
        query.prepare("SELECT inputname "
                      "FROM cardinput "
                      "WHERE sourceid = :SOURCEID AND "
                      "      cardid   = :CARDID");
        query.bindValue(":SOURCEID", sourceid);
    }
    else
    {
        query.prepare("SELECT inputname "
                      "FROM cardinput "
                      "WHERE cardid   = :CARDID");
    }
    query.bindValue(":CARDID",   cardid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputNames()", query);
    }
    else
    {
        while (query.next())
            list.append( query.value(0).toString() );
    }

    return list;
}

bool CardUtil::GetInputInfo(InputInfo &input, vector<uint> *groupids)
{
    if (!input.inputid)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT inputname, sourceid, cardid, livetvorder "
                  "FROM cardinput "
                  "WHERE cardinputid = :INPUTID");
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
    input.cardid   = query.value(2).toUInt();
    input.livetvorder = query.value(3).toUInt();

    if (groupids)
        *groupids = GetInputGroups(input.inputid);

    return true;
}

uint CardUtil::GetCardID(uint inputid)
{
    InputInfo info(QString::null, 0, inputid, 0, 0, 0);
    GetInputInfo(info);
    return info.cardid;
}

QString CardUtil::GetInputName(uint inputid)
{
    InputInfo info(QString::null, 0, inputid, 0, 0, 0);
    GetInputInfo(info);
    return info.name;
}

QString CardUtil::GetStartingChannel(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT startchan "
                  "FROM cardinput "
                  "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetStartingChannel(uint)", query);
    else if (query.next())
        return query.value(0).toString();

    return QString::null;
}

QString CardUtil::GetDisplayName(uint inputid)
{
    if (!inputid)
        return QString::null;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT displayname, cardid, inputname "
                  "FROM cardinput "
                  "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetDisplayName(uint)", query);
    else if (query.next())
    {
        QString result = query.value(0).toString();
        if (result.isEmpty())
            result = QString("%1: %2").arg(query.value(1).toInt())
                                      .arg(query.value(2).toString());
        return result;
    }

    return QString::null;
}

uint CardUtil::GetInputID(uint cardid, const QString &inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid "
                  "FROM cardinput "
                  "WHERE inputname = :INPUTNAME AND "
                  "      cardid    = :CARDID");
    query.bindValue(":INPUTNAME", inputname);
    query.bindValue(":CARDID",    cardid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetInputID(uint,QString)", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

uint CardUtil::GetInputID(uint cardid, uint sourceid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid "
                  "FROM cardinput "
                  "WHERE sourceid  = :SOURCEID AND "
                  "      cardid    = :CARDID");
    query.bindValue(":SOURCEID", sourceid);
    query.bindValue(":CARDID",    cardid);

    if (!query.exec())
        MythDB::DBError("CardUtil::GetInputID(uint,uint)", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

uint CardUtil::GetSourceID(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid "
        "FROM cardinput "
        "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);
    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetSourceID()", query);
    else if (query.next())
        return query.value(0).toUInt();

    return 0;
}

vector<uint> CardUtil::GetAllInputIDs(void)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid "
        "FROM cardinput");

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetAllInputIDs(uint)", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

vector<uint> CardUtil::GetInputIDs(uint cardid)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid "
        "FROM cardinput "
        "WHERE cardid = :CARDID");

    query.bindValue(":CARDID", cardid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetInputIDs(uint)", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

int CardUtil::CreateCardInput(const uint cardid,
                              const uint sourceid,
                              const QString &inputname,
                              const QString &externalcommand,
                              const QString &changer_device,
                              const QString &changer_model,
                              const QString &hostname,
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
        "INSERT INTO cardinput "
        "(cardid, sourceid, inputname, externalcommand, changer_device, "
        "changer_model, tunechan, startchan, displayname, dishnet_eit, "
        "recpriority, quicktune, schedorder, livetvorder) "
        "VALUES (:CARDID, :SOURCEID, :INPUTNAME, :EXTERNALCOMMAND, "
        ":CHANGERDEVICE, :CHANGERMODEL, :TUNECHAN, :STARTCHAN, :DISPLAYNAME, "
        ":DISHNETEIT, :RECPRIORITY, :QUICKTUNE, :SCHEDORDER, :LIVETVORDER ) ");

    query.bindValue(":CARDID", cardid);
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

    query.prepare("SELECT MAX(cardinputid) FROM cardinput");

    if (!query.exec())
    {
        MythDB::DBError("CreateCardInput maxinput", query);
        return -1;
    }

    int inputid = -1; /* must be int not uint because of return type. */

    if (query.next())
        inputid = query.value(0).toInt();

    return inputid;
}

bool CardUtil::DeleteInput(uint inputid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "DELETE FROM cardinput "
        "WHERE cardinputid = :INPUTID");
    query.bindValue(":INPUTID", inputid);

    if (!query.exec())
    {
        MythDB::DBError("DeleteInput", query);
        return false;
    }

    return true;
}

bool CardUtil::DeleteOrphanInputs(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardinputid "
                  "FROM cardinput "
                  "LEFT JOIN capturecard "
                  "ON (capturecard.cardid = cardinput.cardid) "
                  "WHERE capturecard.cardid IS NULL");
    if (!query.exec())
    {
        MythDB::DBError("DeleteOrphanInputs -- query disconnects", query);
        return false;
    }

    bool ok = true;
    while (query.next())
    {
        uint inputid = query.value(0).toUInt();
        if (DeleteInput(inputid))
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("Removed orphan input %1")
                     .arg(inputid));
        }
        else
        {
            ok = false;
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to remove orphan input %1")
                     .arg(inputid));
        }
    }

    return ok;
}

uint CardUtil::CreateInputGroup(const QString &name)
{
    MSqlQuery query(MSqlQuery::InitCon());
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

bool CardUtil::CreateInputGroupIfNeeded(uint cardid)
{
    // Make sure the card's inputs are all in a single
    // input group, create one if needed.
    vector<uint> ingrps = CardUtil::GetSharedInputGroups(cardid);
    vector<uint> inputs = CardUtil::GetInputIDs(cardid);

    if (ingrps.empty() && !inputs.empty())
    {
        QString name = CardUtil::GetRawCardType(cardid) + "_" +
            CardUtil::GetVideoDevice(cardid);
        uint id = 0;
        for (uint i = 0; !id && (i < 100); i++)
        {
            if (i)
                name += QString(":%1").arg(i);
            id = CardUtil::CreateInputGroup(name);
        }
        if (!id)
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create input group");
            return false;
        }

        bool ok = true;
        for (uint i = 0; i < inputs.size(); i++)
            ok &= CardUtil::LinkInputGroup(inputs[i], id);

        if (!ok)
            LOG(VB_GENERAL, LOG_ERR, "Failed to link to new input group");

        return ok;
    }

    return true;
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
            "( SELECT cardinputid FROM cardinput )");
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

vector<uint> CardUtil::GetSharedInputGroups(uint cardid)
{
    vector<uint> list;

    vector<uint> inputs = GetInputIDs(cardid);
    if (inputs.empty())
        return list;

    list = GetInputGroups(inputs[0]);
    for (uint i = 1; (i < inputs.size()) && !list.empty(); i++)
    {
        vector<uint> curlist = GetInputGroups(inputs[i]);
        vector<uint> newlist;
        for (uint j = 0; j < list.size(); j++)
        {
            if (find(curlist.begin(), curlist.end(), list[j]) != curlist.end())
                newlist.push_back(list[j]);
        }
        list = newlist;
    }

    return list;
}

vector<uint> CardUtil::GetGroupCardIDs(uint inputgroupid)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        "SELECT DISTINCT cardid "
        "FROM cardinput, inputgroup "
        "WHERE inputgroupid = :GROUPID AND "
        "      cardinput.cardinputid = inputgroup.cardinputid "
        "ORDER BY cardid");

    query.bindValue(":GROUPID", inputgroupid);

    if (!query.exec())
    {
        MythDB::DBError("CardUtil::GetGroupCardIDs()", query);
        return list;
    }

    while (query.next())
        list.push_back(query.value(0).toUInt());

    return list;
}

vector<uint> CardUtil::GetConflictingCards(uint inputid, uint exclude_cardid)
{
    vector<uint> inputgroupids = CardUtil::GetInputGroups(inputid);

    for (uint i = 0; i < inputgroupids.size(); i++)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("  Group ID %1")
                                     .arg(inputgroupids[i]));
    }

    vector<uint> cardids;
    for (uint i = 0; i < inputgroupids.size(); i++)
    {
        vector<uint> tmp = CardUtil::GetGroupCardIDs(inputgroupids[i]);
        for (uint j = 0; j < tmp.size(); j++)
        {
            if (tmp[j] == exclude_cardid)
                continue;

            if (find(cardids.begin(), cardids.end(), tmp[j]) != cardids.end())
                continue;

            cardids.push_back(tmp[j]);
        }
    }

    for (uint i = 0; i < cardids.size(); i++)
        LOG(VB_RECORD, LOG_INFO, LOC + QString("  Card ID %1").arg(cardids[i]));

    return cardids;
}

bool CardUtil::GetTimeouts(uint cardid,
                           uint &signal_timeout, uint &channel_timeout)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT signal_timeout, channel_timeout "
        "FROM capturecard "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

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

bool CardUtil::IsInNeedOfExternalInputConf(uint cardid)
{
    DiSEqCDev dev;
    DiSEqCDevTree *diseqc_tree = dev.FindTree(cardid);

    bool needsConf = false;
    if (diseqc_tree)
        needsConf = diseqc_tree->IsInNeedOfConf();

    return needsConf;
}

uint CardUtil::GetQuickTuning(uint cardid, const QString &input_name)
{
    uint quicktune = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT quicktune "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
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
    int videofd, QString &card, QString &driver, uint32_t &version,
    uint32_t &capabilities)
{
    card = driver = QString::null;
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
        card = QString::fromAscii((const char*)capability.card);
        driver = QString::fromAscii((const char*)capability.driver);
        version = capability.version;
        capabilities = capability.capabilities;
    }
#ifdef USING_V4L1
    else // Fallback to V4L1 query
    {
        struct video_capability capability;
        if (ioctl(videofd, VIDIOCGCAP, &capability) >= 0)
            card = QString::fromAscii((const char*)capability.name);
    }
#endif // USING_V4L1
#endif // USING_V4L2

    if (!driver.isEmpty())
        driver.remove( QRegExp("\\[[0-9]\\]$") );

    return !card.isEmpty();
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

    // Create an input on single input cards that don't advertise input
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

InputNames CardUtil::GetConfiguredDVBInputs(uint cardid)
{
    InputNames list;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardinputid, inputname "
        "FROM cardinput "
        "WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec() || !query.isActive())
        MythDB::DBError("CardUtil::GetConfiguredDVBInputs", query);
    else
    {
        while (query.next())
            list[query.value(0).toUInt()] = query.value(1).toString();
    }
    return list;
}

QStringList CardUtil::ProbeVideoInputs(QString device, QString cardtype)
{
    QStringList ret;

    if (IsSingleInputCard(cardtype))
        ret += "MPEG2TS";
    else if ("DVB" == cardtype)
        ret += ProbeDVBInputs(device);
    else
        ret += ProbeV4LVideoInputs(device);

    return ret;
}

QStringList CardUtil::ProbeAudioInputs(QString device, QString cardtype)
{
    LOG(VB_GENERAL, LOG_DEBUG, QString("ProbeAudioInputs(%1,%2)")
                                   .arg(device).arg(cardtype));
    QStringList ret;

    if ("HDPVR" == cardtype)
        ret += ProbeV4LAudioInputs(device);

    return ret;
}

QStringList CardUtil::ProbeV4LVideoInputs(QString device)
{
    bool ok;
    QStringList ret;
    QByteArray dev = device.toAscii();
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
    int videofd = open(device.toAscii().constData(), O_RDWR);
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
    uint cardid = CardUtil::GetFirstCardID(device);
    if (!cardid)
        return ret;

    InputNames list = GetConfiguredDVBInputs(cardid);
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

QString CardUtil::GetDeviceLabel(const QString &cardtype,
                                 const QString &videodevice)
{
    return QString("[ %1 : %2 ]").arg(cardtype).arg(videodevice);
}

QString CardUtil::GetDeviceLabel(uint cardid)
{
    QString devlabel;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype, videodevice "
                  "FROM capturecard WHERE cardid = :CARDID ");
    query.bindValue(":CARDID", cardid);

    if (query.exec() && query.next())
    {
        return GetDeviceLabel(query.value(0).toString(),
                              query.value(1).toString());
    }

    return "[ UNKNOWN ]";
}

void CardUtil::GetCardInputs(
    uint                cardid,
    const QString      &device,
    const QString      &cardtype,
    QStringList        &inputLabels,
    vector<CardInput*> &cardInputs)
{
    QStringList inputs;
    bool is_dtv = !IsEncoder(cardtype) && !IsUnscanable(cardtype);

    if (IsSingleInputCard(cardtype))
        inputs += "MPEG2TS";
    else if ("DVB" != cardtype)
        inputs += ProbeV4LVideoInputs(device);

    QString dev_label = GetDeviceLabel(cardtype, device);

    QStringList::iterator it = inputs.begin();
    for (; it != inputs.end(); ++it)
    {
        CardInput *cardinput = new CardInput(is_dtv, false, false, cardid);
        cardinput->loadByInput(cardid, (*it));
        inputLabels.push_back(
            dev_label + QString(" (%1) -> %2")
            .arg(*it).arg(cardinput->getSourceName()));
        cardInputs.push_back(cardinput);
    }

#ifdef USING_DVB
    if ("DVB" == cardtype)
    {
        bool needs_conf = IsInNeedOfExternalInputConf(cardid);
        InputNames list = GetConfiguredDVBInputs(cardid);
        if (!needs_conf && list.empty())
            list[0] = "DVBInput";

        InputNames::const_iterator it;
        for (it = list.begin(); it != list.end(); ++it)
        {
            CardInput *cardinput = new CardInput(is_dtv, true, false, cardid);
            cardinput->loadByInput(cardid, *it);
            inputLabels.push_back(
                dev_label + QString(" (%1) -> %2")
                .arg(*it).arg(cardinput->getSourceName()));
            cardInputs.push_back(cardinput);
        }

        // plus add one "new" input
        if (needs_conf)
        {
            CardInput *newcard = new CardInput(is_dtv, true, true, cardid);
            QString newname = QString("DVBInput #%1").arg(list.size() + 1);
            newcard->loadByInput(cardid, newname);
            inputLabels.push_back(dev_label + " " + QObject::tr("New Input"));
            cardInputs.push_back(newcard);
        }
    }
#endif // USING_DVB
}

int CardUtil::CreateCaptureCard(const QString &videodevice,
                                 const QString &audiodevice,
                                 const QString &vbidevice,
                                 const QString &cardtype,
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
        "VALUES (:VIDEODEVICE, :AUDIODEVICE, :VBIDEVICE, :CARDTYPE, "
        ":AUDIORATELIMIT, :HOSTNAME, :DVBSWFILTER, :DVBSATTYPE, "
        ":DVBWAITFORSEQSTART, :SKIPBTAUDIO, :DVBONDEMAND, :DVBDISEQCTYPE, "
        ":FIREWIRESPEED, :FIREWIREMODEL, :FIREWIRECONNECTION, :SIGNALTIMEOUT, "
        ":CHANNELTIMEOUT, :DVBTUNINGDELAY, :CONTRAST, :BRIGHTNESS, :COLOUR, "
        ":HUE, :DISEQCID, :DVBEITSCAN ) ");

    query.bindValue(":VIDEODEVICE", videodevice);
    query.bindValue(":AUDIODEVICE", audiodevice);
    query.bindValue(":VBIDEVICE", vbidevice);
    query.bindValue(":CARDTYPE", cardtype);
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
        MythDB::DBError("CreateCaptureCard maxcard", query);
        return -1;
    }

    int cardid = -1;  /* must be int not uint because of return type. */

    if (query.next())
        cardid = query.value(0).toInt();

    return cardid;
}

bool CardUtil::DeleteCard(uint cardid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    bool ok = true;

    if (!cardid)
        return true;

    // delete any DiSEqC device tree
    DiSEqCDevTree tree;
    tree.Load(cardid);
    if (!tree.Root())
    {
        tree.SetRoot(NULL);
        tree.Store(cardid);
    }

    // delete any clones
    QString rawtype     = GetRawCardType(cardid);
    QString videodevice = GetVideoDevice(cardid);
    if (IsTunerSharingCapable(rawtype) && !videodevice.isEmpty())
    {
        query.prepare(
            "SELECT cardid "
            "FROM capturecard "
            "WHERE videodevice = :DEVICE AND "
            "      cardid      > :CARDID");
        query.bindValue(":DEVICE", videodevice);
        query.bindValue(":CARDID", cardid);

        if (!query.exec())
        {
            MythDB::DBError("DeleteCard -- find clone cards", query);
            return false;
        }

        while (query.next())
            ok &= DeleteCard(query.value(0).toUInt());

        if (!ok)
            return false;
    }

    // delete inputs
    vector<uint> inputs = CardUtil::GetInputIDs(cardid);
    for (uint i = 0; i < inputs.size(); i++)
        ok &= CardUtil::DeleteInput(inputs[i]);

    if (!ok)
        return false;

    // actually delete the capturecard row for this card
    query.prepare("DELETE FROM capturecard WHERE cardid = :CARDID");
    query.bindValue(":CARDID", cardid);

    if (!query.exec())
    {
        MythDB::DBError("DeleteCard -- delete row", query);
        ok = false;
    }

    if (ok)
    {
        // delete any orphaned inputs & unused input groups
        DeleteOrphanInputs();
        UnlinkInputGroup(0,0);
    }

    return ok;
}

bool CardUtil::DeleteAllCards(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    return (query.exec("TRUNCATE TABLE inputgroup") &&
            query.exec("TRUNCATE TABLE diseqc_config") &&
            query.exec("TRUNCATE TABLE diseqc_tree") &&
            query.exec("TRUNCATE TABLE cardinput") &&
            query.exec("TRUNCATE TABLE capturecard"));
}

vector<uint> CardUtil::GetCardList(void)
{
    vector<uint> list;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT cardid "
        "FROM capturecard "
        "ORDER BY cardid");

    if (!query.exec())
        MythDB::DBError("CardUtil::GetCardList()", query);
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
    hdhr = hdhomerun_device_create_from_str(device.toAscii(), NULL);
    if (!hdhr)
        return false;

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (model && strstr(model, "dvb"))
        return true;
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
    hdhr = hdhomerun_device_create_from_str(device.toAscii(), NULL);
    if (!hdhr)
        return QObject::tr("Invalid Device ID or address.");

    const char *model = hdhomerun_device_get_model_str(hdhr);
    if (!model)
        return connectErr;


    QString   description = model;
    char     *sVersion;
    uint32_t  iVersion;

    if (hdhomerun_device_get_version(hdhr, &sVersion, &iVersion))
        description += QObject::tr(", firmware: %2").arg(sVersion);

    return description;
#else

    (void) device;
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
    (void) device_num;
    if (error)
        *error = "Not compiled with ASI support.";
    return false;
#endif
}

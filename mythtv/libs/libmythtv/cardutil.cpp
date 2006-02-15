// Standard UNIX C headers
#include <fcntl.h>
#include <sys/ioctl.h>

// MythTV headers
#include "cardutil.h"
#include "videosource.h"
#include "mythcontext.h"
#include "mythdbcon.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbdev.h"
#endif

#ifdef USING_V4L
#include "videodev_myth.h"
#endif

const QString CardUtil::DVB = "DVB";

/** \fn CardUtil::IsCardTypePresent(const QString&)
 *  \brief Returns true if the card type is present
 *  \param strType card type being checked for
 */
bool CardUtil::IsCardTypePresent(const QString &strType)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT count(cardtype) "
                  "FROM capturecard, cardinput "
                  "WHERE cardinput.cardid = capturecard.cardid AND "
                  "      capturecard.cardtype = :CARDTYPE AND "
                  "      capturecard.hostname = :HOSTNAME");
    query.bindValue(":CARDTYPE", strType);
    query.bindValue(":HOSTNAME", gContext->GetHostName());

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        int count = query.value(0).toInt();

        if (count > 0)
            return true;
    }

    return false;
}

/** \fn CardUtil::GetDVBType(uint, QString&, QString&)
 *  \brief Returns the card type from the video device
 *  \param device    DVB videodevice to be checked
 *  \param name      Returns the probed card name
 *  \param card_type Returns the card type as a string
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetDVBType(
    uint device, QString &name, QString &card_type)
{
    (void)device;
    (void)name;
    (void)card_type;

    CARD_TYPES nRet = ERROR_OPEN;
#ifdef USING_DVB
    int fd_frontend = open(dvbdevice(DVB_DEV_FRONTEND, device),
                           O_RDWR | O_NONBLOCK);
    if (fd_frontend >= 0)
    {
        struct dvb_frontend_info info;
        nRet = ERROR_PROBE;
        if (ioctl(fd_frontend, FE_GET_INFO, &info) >= 0)
        {
            name = info.name;
            switch(info.type)
            {
            case FE_QAM:
                nRet = QAM;
                card_type = "QAM";
                break;
            case FE_QPSK:
                nRet = QPSK;
                card_type = "QPSK";
                break;
            case FE_OFDM:
                nRet = OFDM;
                card_type = "OFDM";
                break;
#if (DVB_API_VERSION_MINOR == 1)
            case FE_ATSC:
                nRet = ATSC;
                card_type = "ATSC";
                break;
#endif
            }
        }
        close(fd_frontend);
    } 
#endif
    return nRet;
}

/** \fn CardUtil::HasDVBCRCBug(uint)
 *  \brief Returns true if and only if the device munges 
 *         PAT/PMT tables, and then doesn't fix the CRC.
 *
 *   Currently the list of broken DVB hardware and drivers includes:
 *   "Philips TDA10046H DVB-T", "VLSI VES1x93 DVB-S", and "ST STV0299 DVB-S"
 *
 *  \param device video dev to be checked
 *  \return true iff the device munges tables, so that they fail a CRC check.
 */
bool CardUtil::HasDVBCRCBug(uint device)
{
    QString name(""), type("");
    GetDVBType(device, name, type);
    return ((name == "Philips TDA10046H DVB-T") || // munges PMT
            (name == "VLSI VES1x93 DVB-S")      || // munges PMT
            (name == "DST DVB-S")               || // munges PAT
            (name == "ST STV0299 DVB-S"));         // munges PAT
}

/** \fn CardUtil::GetCardType(uint, QString&, QString&)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \param name      Returns the probed card name
 *  \param card_type Returns the card type as a string
 *  \return the card type from CARD_TYPES enum
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID, QString &name,
                                                QString &card_type)
{
    CARD_TYPES nRet = ERROR_OPEN;
    QString strDevice;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, cardtype "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        strDevice = query.value(0).toString();
        card_type = query.value(1).toString().upper();
    }
    else
        return nRet;

    if (card_type == "V4L")
        nRet = V4L;
    else if (card_type == "MPEG")
        nRet = MPEG;
    else if (card_type == "FIREWIRE")
        nRet = FIREWIRE;
    else if (card_type == "HDTV")
        nRet = HDTV;
#ifdef USING_DVB
    else if (card_type == "DVB")
        nRet = GetDVBType(strDevice.toInt(), name, card_type);
#else
    (void)name;
#endif
    return nRet;
}

/** \fn CardUtil::GetCardType(uint, QString&)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \param name      Returns the probed card name
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID, QString &name)
{
    QString card_type;
    return CardUtil::GetCardType(nCardID, name, card_type);
}

/** \fn CardUtil::GetCardType(uint)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \return the card type
 */
enum CardUtil::CARD_TYPES CardUtil::GetCardType(uint nCardID)
{
    QString name, card_type;
    return CardUtil::GetCardType(nCardID, name, card_type);
}

/** \fn CardUtil::IsDVBCardType(const QString)
 *  \brief Returns true iff the card_type is one of the DVB types.
 */
bool CardUtil::IsDVBCardType(const QString card_type)
{
    QString ct = card_type.upper();
    return (ct == "DVB") || (ct == "QAM") || (ct == "QPSK") ||
        (ct == "OFDM") || (ct == "ATSC");
}

/** \fn CardUtil::GetVideoDevice(uint, QString&)
 *  \brief Returns the card type from the video device
 *  \param nCardID   cardid of card to be checked
 *  \param device    Returns the videodevice corresponding to cardid
 *  \return the card type
 */
bool CardUtil::GetVideoDevice(uint nCardID, QString& device)
{
    bool fRet=false;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        device = query.value(0).toString();
        fRet = true;
    }
    return fRet;
}

/** \fn CardUtil::GetCardID(const QString&, QString)
 *  \brief Returns the cardid of the card that uses the specified
 *         videodevice, and optionally a non-local hostname.
 *  \param videodevice Video device we want card id for
 *  \param hostname    Host on which device resides, only
 *                     required if said host is not the localhost
 */
int CardUtil::GetCardID(const QString &videodevice, QString hostname)
{
    if (hostname == QString::null)
        hostname = gContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery =
        QString("SELECT cardid FROM capturecard "
                "WHERE videodevice = '%1' AND "
                "      hostname = '%2'")
        .arg(videodevice).arg(hostname);

    query.prepare(thequery);
    if (!query.exec() || !query.isActive() || query.size() <= 0)
    {
        MythContext::DBError("CardUtil::GetCardID()", query);
        return -1;
    }
    query.next();
    return query.value(0).toInt();
}

/** \fn CardUtil::GetVideoDevice(uint, QString&, QString&)
 *  \brief Returns the the video device associated with the card id
 *  \param nCardID card id to check
 *  \param device the returned device
 *  \param vbi the returned vbi device
 *  \return true on success
 */
bool CardUtil::GetVideoDevice(uint nCardID, QString& device, QString& vbi)
{
    bool fRet=false;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT videodevice, vbidevice "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        device = query.value(0).toString();
        vbi = query.value(1).toString();
        fRet = true;
    }
    return fRet;
}

/** \fn CardUtil::IsDVB(uint)
 *  \brief Returns true if the card is a DVB card
 *  \param nCardID card id to check
 *  \return true if the card is a DVB one
 */
bool CardUtil::IsDVB(uint nCardID)
{
    bool fRet = false;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT cardtype "
                  "FROM capturecard "
                  "WHERE capturecard.cardid= :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        if (query.value(0).toString() == "DVB")
           fRet = true;
    }
    return fRet;
}

/** \fn CardUtil::GetDISEqCType(uint)
 *  \brief Returns the disqec type associated with a DVB card
 *  \param nCardID card id to check
 *  \return the disqec type
 */
enum DISEQC_TYPES CardUtil::GetDISEqCType(uint nCardID)
{
    int iRet = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT dvb_diseqc_type "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        iRet = query.value(0).toInt();
    }
    return (DISEQC_TYPES)iRet;
}

/** \fn CardUtil::GetDefaultInput(uint)
 *  \brief Returns the default input for the card
 *  \param nCardID card id to check
 *  \return the default input
 */
QString CardUtil::GetDefaultInput(uint nCardID)
{
    QString str;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT defaultinput "
                  "FROM capturecard "
                  "WHERE capturecard.cardid = :CARDID");
    query.bindValue(":CARDID", nCardID);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        str = query.value(0).toString();
    }
    return str;
}

bool CardUtil::IgnoreEncrypted(uint cardid, const QString &input_name)
{
    bool freetoair = true;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        QString("SELECT freetoaironly FROM cardinput "
                "WHERE cardid='%1' AND inputname='%2'")
        .arg(cardid).arg(input_name));

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        query.next();
        freetoair = query.value(0).toBool();
    }
    //VERBOSE(VB_IMPORTANT,
    //        QString("CardUtil::IgnoreEncrypted(%1, %2) -> %3")
    //        .arg(cardid).arg(input_name).arg(freetoair));
    return freetoair;
}

bool CardUtil::TVOnly(uint cardid, const QString &input_name)
{
    bool radioservices = true;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT radioservices "
        "FROM cardinput "
        "WHERE cardid    = :CARDID AND "
        "      inputname = :INPUTNAME");
    query.bindValue(":CARDID",    cardid);
    query.bindValue(":INPUTNAME", input_name);

    if (query.exec() && query.isActive() && query.next())
        radioservices = query.value(0).toBool();

    return !radioservices;
}

bool CardUtil::hasV4L2(int videofd)
{
    (void) videofd;
#ifdef USING_V4L
    struct v4l2_capability vcap;
    bzero(&vcap, sizeof(vcap));

    return ((ioctl(videofd, VIDIOC_QUERYCAP, &vcap) >= 0) &&
            (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE));
#else // if !USING_V4L
    return false;
#endif // !USING_V4L
}

InputNames CardUtil::probeV4LInputs(int videofd, bool &ok)
{
    (void) videofd;

    InputNames list;
    ok = false;

#ifdef USING_V4L
    bool usingv4l2 = hasV4L2(videofd);

    // V4L v2 query
    struct v4l2_input vin;
    bzero(&vin, sizeof(vin));
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

    // V4L v1 query
    struct video_capability vidcap;
    bzero(&vidcap, sizeof(vidcap));
    if (ioctl(videofd, VIDIOCGCAP, &vidcap) != 0)
    {
        QString msg = QObject::tr("Could not query inputs.");
        VERBOSE(VB_IMPORTANT, msg + ENO);
        list[-1] = msg;
        vidcap.channels = 0;
    }

    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        bzero(&test, sizeof(test));
        test.channel = i;

        if (ioctl(videofd, VIDIOCGCHAN, &test) != 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Could determine name of input #%1"
                            "\n\t\t\tNot adding it to the list.")
                    .arg(test.channel) + ENO);
            continue;
        }

        list[i] = test.name;
    }

    // Create an input on single input cards that don't advertise input
    if (!list.size())
        list[0] = "Television";

    ok = true;
#else // if !USING_V4L
    list[-1] += QObject::tr("ERROR, Compile with V4L support to query inputs");
#endif // !USING_V4L
    return list;
}

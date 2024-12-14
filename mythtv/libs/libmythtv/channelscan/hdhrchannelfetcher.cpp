// Std C headers
#include <cmath>
#include <unistd.h>
#include <utility>

// Qt headers
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QDomDocument>

#ifdef USING_HDHOMERUN
#include HDHOMERUN_HEADERFILE
#endif

// MythTV headers
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "cardutil.h"
#include "channelutil.h"
#include "libmyth/mythcontext.h"
#include "scanmonitor.h"
#include "hdhrchannelfetcher.h"

#define LOC QString("HDHRChanFetch: ")

namespace {

constexpr const char* QUERY_CHANNELS
{ "http://{IP}/lineup.xml?tuning" };

QString getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return {};
}

QString getStrValue(const QDomElement &element, const QString &name, int index=0)
{
    QDomNodeList nodes = element.elementsByTagName(name);
    if (!nodes.isEmpty())
    {
        if (index >= nodes.count())
            index = 0;
        QDomElement e = nodes.at(index).toElement();
        return getFirstText(e);
    }
    return {};
}

int getIntValue(const QDomElement &element, const QString &name, int index=0)
{
    QString value = getStrValue(element, name, index);
    return value.toInt();
}

bool sendQuery(const QString& query, QDomDocument* xmlDoc)
{
    QByteArray result;

    if (!GetMythDownloadManager()->download(query, &result, true))
        return false;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
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
#else
    auto parseResult = xmlDoc->setContent(result);
    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Error parsing: %1\nat line: %2  column: %3 msg: %4").
                arg(query).arg(parseResult.errorLine).arg(parseResult.errorColumn)
               .arg(parseResult.errorMessage));
        return false;
    }
#endif

    // Check for a status or error element
    QDomNodeList statusNodes = xmlDoc->elementsByTagName("Status");

    if (!statusNodes.count())
        statusNodes = xmlDoc->elementsByTagName("Error");

    if (statusNodes.count())
    {
        QDomElement elem = statusNodes.at(0).toElement();
        if (!elem.isNull())
        {
            int errorCode = getIntValue(elem, "ErrorCode");
            QString errorDesc = getStrValue(elem, "ErrorDescription");

            if (errorCode == 0 /* SUCCESS */)
                return true;

            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("API Error: %1 - %2, Query was: %3").arg(errorCode).arg(errorDesc, query));

            return false;
        }
    }

    // No error detected so assume we got a valid xml result
    return true;
}

hdhr_chan_map_t *getChannels(const QString& ip)
{
    auto *result = new hdhr_chan_map_t;
    auto *xmlDoc = new QDomDocument();
    QString query = QUERY_CHANNELS;

    query.replace("{IP}", ip);

    if (!sendQuery(query, xmlDoc))
    {
        delete xmlDoc;
        delete result;
        return nullptr;
    }

    QDomNodeList chanNodes = xmlDoc->elementsByTagName("Program");

    for (int x = 0; x < chanNodes.count(); x++)
    {
        QDomElement chanElem = chanNodes.at(x).toElement();
        QString guideName = getStrValue(chanElem, "GuideName");
        QString guideNumber = getStrValue(chanElem, "GuideNumber");
        QString url = getStrValue(chanElem, "URL");
        QString modulation = getStrValue(chanElem, "Modulation");
        QString videoCodec = getStrValue(chanElem, "VideoCodec");
        QString audioCodec = getStrValue(chanElem, "AudioCodec");
        uint frequency = getStrValue(chanElem, "Frequency").toUInt();
        uint serviceID = getStrValue(chanElem, "ProgramNumber").toUInt();
        uint transportID = getStrValue(chanElem, "TransportStreamID").toUInt();
        QString onid = getStrValue(chanElem, "OriginalNetworkID");

        // Fixup: Remove leading colon in network ID:
        // <OriginalNetworkID>:8720</OriginalNetworkID>
        // This looks a bug in the XML representation, N.B. bug not present in JSON.
        onid.replace(":", "");
        uint originalNetworkID = onid.toUInt();

        LOG(VB_CHANSCAN, LOG_DEBUG, LOC + QString("ONID/TID/SID %1 %2 %3")
            .arg(originalNetworkID).arg(transportID).arg(serviceID));

        HDHRChannelInfo chanInfo(guideName, guideNumber, url, modulation, videoCodec,
            audioCodec, frequency, serviceID, originalNetworkID, transportID);

        result->insert(guideNumber, chanInfo);
    }
    return result;
}

QString HDHRIPv4Address([[maybe_unused]] const QString &device)
{
#ifdef USING_HDHOMERUN
    hdhomerun_device_t *hdhr =
        hdhomerun_device_create_from_str(device.toLatin1(), nullptr);
    if (!hdhr)
        return {};

    uint32_t ipv4 = hdhomerun_device_get_device_ip(hdhr);
    hdhomerun_device_destroy(hdhr);

    if (!ipv4)
        return {};

    return QString("%1.%2.%3.%4").arg(ipv4>>24&0xff).arg(ipv4>>16&0xff).arg(ipv4>>8&0xff).arg(ipv4&0xff);
#else
    return {};
#endif
}

// Examples of hdhrmod values: a8qam64-6875 a8qam256-6900 t8dvbt2 8vsb
DTVModulationSystem HDHRMod2Modsys(const QString& hdhrmod)
{
    if (hdhrmod.contains("dvbt2"))
        return DTVModulationSystem(DTVModulationSystem::kModulationSystem_DVBT2);
    if (hdhrmod.contains("dvbt"))
        return DTVModulationSystem(DTVModulationSystem::kModulationSystem_DVBT);
    if (hdhrmod.startsWith("a8qam"))
        return DTVModulationSystem(DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A);
    if (hdhrmod.contains("vsb"))
        return DTVModulationSystem(DTVModulationSystem::kModulationSystem_ATSC);
    if (hdhrmod.contains("psk"))
        return DTVModulationSystem(DTVModulationSystem::kModulationSystem_ATSC);
    return DTVModulationSystem(DTVModulationSystem::kModulationSystem_UNDEFINED);
}

signed char HDHRMod2Bandwidth(const QString& hdhrmod)
{
    if (hdhrmod.startsWith("t8") || hdhrmod.startsWith("a8"))
        return '8';
    if (hdhrmod.startsWith("t7") || hdhrmod.startsWith("a7"))
        return '7';
    if (hdhrmod.startsWith("t6") || hdhrmod.startsWith("a6"))
        return '6';
    return 'a';
}

uint HDHRMod2SymbolRate(const QString& hdhrmod)
{
    static const QRegularExpression re(R"(^(a8qam\d+-)(\d+))");
    QRegularExpressionMatch match = re.match(hdhrmod);
    if (match.hasMatch())
    {
        QString matched = match.captured(2);
        return matched.toUInt() * 1000;
    }
    return 0;
}

QString HDHRMod2Modulation(const QString& hdhrmod)
{
    if (hdhrmod.contains("qam256"))
        return DTVModulation(DTVModulation::kModulationQAM256).toString();
    if (hdhrmod.contains("qam128"))
        return DTVModulation(DTVModulation::kModulationQAM128).toString();
    if (hdhrmod.contains("qam64"))
        return DTVModulation(DTVModulation::kModulationQAM64).toString();
    if (hdhrmod.contains("qam16"))
        return DTVModulation(DTVModulation::kModulationQAM16).toString();
    if (hdhrmod.contains("qpsk"))
        return DTVModulation(DTVModulation::kModulationQPSK).toString();
    if (hdhrmod.contains("8vsb"))
        return DTVModulation(DTVModulation::kModulation8VSB).toString();
    return DTVModulation(DTVModulation::kModulationQAMAuto).toString();
}

void  HDHRMajorMinorChannel(QString channum, uint &atsc_major_channel, uint &atsc_minor_channel)
{
    if (channum.contains("."))
    {
        QChar dot;
        QTextStream(&channum) >> atsc_major_channel >> dot >> atsc_minor_channel;
    }
}

} // namespace

HDHRChannelFetcher::HDHRChannelFetcher(uint cardid, QString inputname, uint sourceid,
                                       ServiceRequirements serviceType, ScanMonitor *monitor) :
    m_scanMonitor(monitor),
    m_cardId(cardid),
    m_inputName(std::move(inputname)),
    m_sourceId(sourceid),
    m_serviceType(serviceType),
    m_thread(new MThread("HDHRChannelFetcher", this))
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Has ScanMonitor %1")
        .arg(monitor?"true":"false"));
}

HDHRChannelFetcher::~HDHRChannelFetcher()
{
    Stop();
    delete m_thread;
    m_thread = nullptr;
    delete m_channels;
    m_channels = nullptr;
}

/** \fn HDHRChannelFetcher::Stop(void)
 *  \brief Stops the scanning thread running
 */
void HDHRChannelFetcher::Stop(void)
{
    m_lock.lock();

    while (m_threadRunning)
    {
        m_stopNow = true;
        m_lock.unlock();
        m_thread->wait(5ms);
        m_lock.lock();
    }

    m_lock.unlock();

    m_thread->wait();
}

hdhr_chan_map_t HDHRChannelFetcher::GetChannels(void)
{
    while (!m_thread->isFinished())
        m_thread->wait(500ms);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Found %1 channels")
        .arg(m_channels->size()));
    return *m_channels;
}

void HDHRChannelFetcher::Scan(void)
{
    Stop();
    m_stopNow = false;
    m_thread->start();
}

void HDHRChannelFetcher::run(void)
{
    m_lock.lock();
    m_threadRunning = true;
    m_lock.unlock();

    bool usingCableCard = CardUtil::IsCableCardPresent(m_cardId, QString("HDHOMERUN"));

    // Step 1/3 : Get the IP of the HDHomeRun to query
    QString dev = CardUtil::GetVideoDevice(m_cardId);
    QString ip = HDHRIPv4Address(dev);

    if (m_stopNow || ip.isEmpty())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Failed to get IP address from videodev (%1)").arg(dev));
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("HDHomeRun IP: %1").arg(ip));

    // Step 2/3 : Download
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(5);
        m_scanMonitor->ScanAppendTextToLog(tr("Downloading Channel List"));
    }

    delete m_channels;
    m_channels = getChannels(ip);

    if (m_stopNow || !m_channels)
    {
        if (!m_channels && m_scanMonitor)
        {
            m_scanMonitor->ScanAppendTextToLog(QCoreApplication::translate("(Common)", "Error"));
            m_scanMonitor->ScanPercentComplete(100);
            m_scanMonitor->ScanErrored(tr("Downloading Channel List Failed"));
        }
        QMutexLocker locker(&m_lock);
        m_threadRunning = false;
        m_stopNow = true;
        return;
    }

    // Step 3/3 : Process
    if (m_scanMonitor)
    {
        m_scanMonitor->ScanPercentComplete(35);
        m_scanMonitor->ScanAppendTextToLog(tr("Adding Channels"));
    }
    SetTotalNumChannels(m_channels->size());
    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Found %1 channels").arg(m_channels->size()));

    // Add the channels to the DB
    hdhr_chan_map_t::const_iterator it = m_channels->cbegin();
    for (uint i = 1; it != m_channels->cend(); ++it, ++i)
    {
        const QString& channum = it.key();
        QString name        = (*it).m_name;
        uint serviceID      = (*it).m_serviceID;
        QString channelType = (*it).m_channelType;
        QString hdhrmod     = (*it).m_modulation;
        uint networkID      = (*it).m_networkID;
        uint transportID    = (*it).m_transportID;
        uint frequency      = (*it).m_frequency;

        DTVModulationSystem modsys = HDHRMod2Modsys(hdhrmod);
        QString modulation = HDHRMod2Modulation(hdhrmod);
        uint symbolrate = HDHRMod2SymbolRate(hdhrmod);
        signed char bandwidth = HDHRMod2Bandwidth(hdhrmod);
        uint atsc_major_channel = 0;
        uint atsc_minor_channel = 0;
        HDHRMajorMinorChannel(channum, atsc_major_channel, atsc_minor_channel);
        QString sistandard = (atsc_major_channel > 0 && atsc_minor_channel > 0) ? "atsc" : "dvb";

        bool use_on_air_guide = true;

        QString msg = tr("%1 channel %2: %3").arg(channelType).arg(channum, -5, QChar(' ')).arg(name, -15, QChar(' '));
        LOG(VB_CHANSCAN, LOG_INFO, QString("Found %1").arg(msg));

        if ((channelType == "Radio") && (m_serviceType & kRequireVideo))
        { // NOLINT(bugprone-branch-clone)
            // Ignore this radio channel
            if (m_scanMonitor)
            {
                m_scanMonitor->ScanAppendTextToLog(tr("Ignoring %1").arg(msg));
            }
        }
        else if ((channelType == "Data") && (m_serviceType != kRequireNothing))
        {
            // Ignore this data channel
            if (m_scanMonitor)
            {
                m_scanMonitor->ScanAppendTextToLog(tr("Ignoring %1").arg(msg));
            }
        }
        else
        {
            // This is a TV channel or another channel type that we want
            int chanid = ChannelUtil::GetChanID(m_sourceId, channum);
            bool adding_channel = chanid <= 0;

            if (adding_channel)
                chanid = ChannelUtil::CreateChanID(m_sourceId, channum);

            if (m_scanMonitor)
            {
                if (adding_channel)
                {
                    m_scanMonitor->ScanAppendTextToLog(tr("Adding %1").arg(msg));
                }
                else
                {
                    m_scanMonitor->ScanAppendTextToLog(tr("Updating %1").arg(msg));
                }
            }

            uint mplexID {0};
            QString freqID;

            if (usingCableCard)
            {
                // With a CableCard, we're going to use virtual
                // channel tuning, so no dtv_multiplex is needed.
                mplexID = 0;
                // When virtual channel tuning, vchan is acquired from freqid field
                freqID = channum;
            }
            else
            {
                // A new dtv_multiplex entry will be created if necessary, otherwise an existing one is returned
                mplexID = ChannelUtil::CreateMultiplex(m_sourceId, sistandard, frequency, modulation,
                                                       transportID, networkID, symbolrate, bandwidth,
                                                       'v', 'a', 'a', QString(), QString(), 'a', QString(),
                                                       QString(), QString(), modsys.toString(), "0.35");
                if (mplexID == 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("No multiplex for %1 sid:%2 freq:%3 url:%4")
                        .arg(msg).arg(serviceID, -5, 10, QChar(' ')).arg(frequency).arg((*it).m_tuning.GetDataURL().toString()));
                    continue;
                }
            }

            if (adding_channel)
            {
                ChannelUtil::CreateChannel(mplexID, m_sourceId, chanid, name, name,
                                        channum, serviceID, atsc_major_channel, atsc_minor_channel,
                                        use_on_air_guide, kChannelVisible, freqID,
                                        QString(), "Default", QString());

                ChannelUtil::CreateIPTVTuningData(chanid, (*it).m_tuning);
            }
            else
            {
                ChannelUtil::UpdateChannel(mplexID, m_sourceId, chanid, name, name,
                                        channum, serviceID, atsc_major_channel, atsc_minor_channel,
                                        use_on_air_guide, kChannelVisible, freqID,
                                        QString(), "Default", QString());

                ChannelUtil::UpdateIPTVTuningData(chanid, (*it).m_tuning);
            }
            LOG(VB_GENERAL, LOG_INFO, QString("%1 sid:%2 freq:%3 url:%4")
                .arg(msg).arg(serviceID, -5, 10, QChar(' ')).arg(frequency).arg((*it).m_tuning.GetDataURL().toString()));
        }
        SetNumChannelsInserted(i);
    }

    if (m_scanMonitor)
    {
        m_scanMonitor->ScanAppendTextToLog(tr("Done"));
        m_scanMonitor->ScanPercentComplete(100);
        m_scanMonitor->ScanComplete();
    }

    QMutexLocker locker(&m_lock);
    m_threadRunning = false;
    m_stopNow = true;
}

void HDHRChannelFetcher::SetNumChannelsInserted(uint val)
{
    uint minval = 70;
    uint range = 100 - minval;
    uint pct = minval + (uint) truncf((((float)val) / m_chanCnt) * range);
    if (m_scanMonitor)
        m_scanMonitor->ScanPercentComplete(pct);
}

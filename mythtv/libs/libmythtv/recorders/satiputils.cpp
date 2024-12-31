// C++
#include <chrono>
#include <thread>

// Qt
#include <QString>
#include <QStringList>

// MythTV headers
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"
#include "libmythupnp/ssdp.h"

#include "cardutil.h"
#include "satiputils.h"

#define LOC QString("SatIP: ")

namespace {
    const QString SATIP_URI = "urn:ses-com:device:SatIPServer:1";
    constexpr std::chrono::milliseconds SEARCH_TIME_MS { 3s };
}

QStringList SatIP::probeDevices(void)
{
    const std::chrono::milliseconds totalSearchTime = SEARCH_TIME_MS;
    auto seconds = duration_cast<std::chrono::seconds>(totalSearchTime);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using UPNP to search for Sat>IP servers (%1 secs)")
        .arg(seconds.count()));

    SSDP::Instance()->PerformSearch(SATIP_URI, seconds);

    MythTimer totalTime; totalTime.start();
    MythTimer searchTime; searchTime.start();

    while (totalTime.elapsed() < totalSearchTime)
    {
        std::this_thread::sleep_for(25ms);
        std::chrono::milliseconds ttl = totalSearchTime - totalTime.elapsed();
        if (searchTime.elapsed() > 249ms && ttl > 1s)
        {
            auto ttl_s = duration_cast<std::chrono::seconds>(ttl);
            LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("UPNP search %1 ms")
                .arg(ttl_s.count()));
            SSDP::Instance()->PerformSearch(SATIP_URI, ttl_s);
            searchTime.start();
        }
    }

    return SatIP::doUPNPsearch(true);
}

QStringList SatIP::doUPNPsearch(bool loginfo)
{
    QStringList result;

    SSDPCacheEntries *satipservers = SSDP::Find(SATIP_URI);

    if (!satipservers)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No UPnP Sat>IP servers found");
        return {};
    }

    int count = satipservers->Count();
    if (count > 0)
    {
        if (loginfo)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found %1 possible Sat>IP servers").arg(count));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No UPnP Sat>IP servers found, but SSDP::Find() != NULL");
    }

    EntryMap map;
    satipservers->GetEntryMap(map);

    for (auto *BE : std::as_const(map))
    {
        QString friendlyName = BE->GetFriendlyName();
        UPnpDeviceDesc *desc = BE->GetDeviceDesc();

        if (!desc)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("GetDeviceDesc() failed for %1").arg(friendlyName));
            continue;
        }

        QString ip = desc->m_hostUrl.host();
        QString id = desc->m_rootDevice.GetUDN();
        QList<NameValue> extraAttribs = desc->m_rootDevice.m_lstExtra;

        for (const auto& attrib : extraAttribs)
        {
            if (attrib.m_sName == "satip:X_SATIPCAP")
            {
                QStringList caps = attrib.m_sValue.split(",");

                for (const auto& cap : std::as_const(caps))
                {
                    QStringList tuner = cap.split("-");

                    if (tuner.size() != 2)
                        continue;

                    int num_tuners = tuner.at(1).toInt();
                    for (int i = 0; i < num_tuners; i++)
                    {
                        QString device = QString("%1 %2 %3 %4 %5")
                                            .arg(id,
                                                 friendlyName.remove(" "),
                                                 ip,
                                                 QString::number(i),
                                                 tuner.at(0));
                        result << device;
                        if (loginfo)
                        {
                            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found %1").arg(device));
                        }
                    }
                }
            }
        }
        BE->DecrRef();
    }

    satipservers->DecrRef();
    satipservers = nullptr;

    return result;
}

QStringList SatIP::findServers(void)
{
    QStringList devs;
    SSDPCacheEntries *satipservers = SSDP::Find(SATIP_URI);
    if (satipservers && satipservers->Count() > 0)
    {
        devs = SatIP::doUPNPsearch(false);
    }
    else
    {
        devs = SatIP::probeDevices();
    }
    return devs;
}

QString SatIP::findDeviceIP(const QString& deviceuuid)
{
    QStringList devs = SatIP::findServers();

    for (const auto& dev : std::as_const(devs))
    {
        QStringList devinfo = dev.split(" ");
        const QString& id = devinfo.at(0);

        if (id.toUpper() == deviceuuid.toUpper())
        {
            return devinfo.at(2);
        }
    }
    return nullptr;
}

CardUtil::INPUT_TYPES SatIP::toDVBInputType(const QString& deviceid)
{
    QStringList dev = deviceid.split(":");
    if (dev.length() < 3)
    {
        return CardUtil::INPUT_TYPES::ERROR_UNKNOWN;
    }

    QString type = dev.at(2).toUpper();
    if (type == "DVBC")
    {
        return CardUtil::INPUT_TYPES::DVBC;
    }
    if (type == "DVBC2")
    {
        return CardUtil::INPUT_TYPES::DVBC; // DVB-C2 is not supported yet.
    }
    if (type == "DVBT")
    {
        return CardUtil::INPUT_TYPES::DVBT;
    }
    if (type == "DVBT2")
    {
        return CardUtil::INPUT_TYPES::DVBT2;
    }
    if (type == "DVBS2")
    {
        return CardUtil::INPUT_TYPES::DVBS2;
    }
    return CardUtil::INPUT_TYPES::ERROR_UNKNOWN;
}

int SatIP::toTunerType(const QString& deviceid)
{
    QStringList devinfo = deviceid.split(":");
    if (devinfo.length() < 3)
    {
        return DTVTunerType::kTunerTypeUnknown;
    }

    QString type = devinfo.at(2).toUpper();

    if (type.startsWith("DVBC")) // DVB-C2 is not supported yet.
    {
        return DTVTunerType::kTunerTypeDVBC;
    }
    if (type == "DVBT")
    {
        return DTVTunerType::kTunerTypeDVBT;
    }
    if (type == "DVBT2")
    {
        return DTVTunerType::kTunerTypeDVBT2;
    }
    if (type == "DVBS")
    {
        return DTVTunerType::kTunerTypeDVBS1;
    }
    if (type == "DVBS2")
    {
        return DTVTunerType::kTunerTypeDVBS2;
    }

    return DTVTunerType::kTunerTypeUnknown;
}

QString SatIP::bw(DTVBandwidth bw)
{
    if (bw == DTVBandwidth::kBandwidth5MHz)
    {
        return "5";
    }
    if (bw == DTVBandwidth::kBandwidth6MHz)
    {
        return "6";
    }
    if (bw == DTVBandwidth::kBandwidth7MHz)
    {
        return "7";
    }
    if (bw == DTVBandwidth::kBandwidth8MHz)
    {
        return "8";
    }
    if (bw == DTVBandwidth::kBandwidth10MHz)
    {
        return "10";
    }
    if (bw == DTVBandwidth::kBandwidth1712kHz)
    {
        return "1.712";
    }
    if (bw == DTVBandwidth::kBandwidthAuto)
    {
        return "auto";
    }
    return "auto";
}

QString SatIP::freq(uint64_t freq)
{
    return QString::number(freq / 1000000.0, 'f', 2);
}

QString SatIP::msys(DTVModulationSystem msys)
{
    if (msys == DTVModulationSystem::kModulationSystem_DVBS)
    {
        return "dvbs";
    }
    if (msys == DTVModulationSystem::kModulationSystem_DVBS2)
    {
        return "dvbs2";
    }
    if (msys == DTVModulationSystem::kModulationSystem_DVBT)
    {
        return "dvbt";
    }
    if (msys == DTVModulationSystem::kModulationSystem_DVBT2)
    {
        return "dvbt2";
    }
    if (msys == DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A)
    {
        return "dvbc";
    }
    // Not supported yet: DVB-C2
    return "auto";
}

QString SatIP::mtype(DTVModulation mtype)
{
    if (mtype == DTVModulation::kModulationQPSK)
    {
        return "qpsk";
    }
    if (mtype == DTVModulation::kModulation8PSK)
    {
        return "8psk";
    }
    if (mtype == DTVModulation::kModulationQAM16)
    {
        return "16qam";
    }
    if (mtype == DTVModulation::kModulationQAM32)
    {
        return "32qam";
    }
    if (mtype == DTVModulation::kModulationQAM64)
    {
        return "64qam";
    }
    if (mtype == DTVModulation::kModulationQAM128)
    {
        return "128qam";
    }
    if (mtype == DTVModulation::kModulationQAM256)
    {
        return "256qam";
    }
    return "auto";
}

QString SatIP::tmode(DTVTransmitMode tmode)
{
    if (tmode == DTVTransmitMode::kTransmissionMode2K)
    {
        return "2k";
    }
    if (tmode == DTVTransmitMode::kTransmissionMode4K)
    {
        return "4k";
    }
    if (tmode == DTVTransmitMode::kTransmissionMode8K)
    {
        return "8k";
    }
    if (tmode == DTVTransmitMode::kTransmissionMode1K)
    {
        return "1k";
    }
    if (tmode == DTVTransmitMode::kTransmissionMode16K)
    {
        return "16k";
    }
    if (tmode == DTVTransmitMode::kTransmissionMode32K)
    {
        return "32k";
    }
    return "auto";
}

QString SatIP::gi(DTVGuardInterval gi)
{
    if (gi == DTVGuardInterval::kGuardInterval_1_4)
    {
        return "14";
    }
    if (gi == DTVGuardInterval::kGuardInterval_1_8)
    {
        return "18";
    }
    if (gi == DTVGuardInterval::kGuardInterval_1_16)
    {
        return "116";
    }
    if (gi == DTVGuardInterval::kGuardInterval_1_32)
    {
        return "132";
    }
    if (gi == DTVGuardInterval::kGuardInterval_1_128)
    {
        return "1128";
    }
    if (gi == DTVGuardInterval::kGuardInterval_19_128)
    {
        return "19128";
    }
    if (gi == DTVGuardInterval::kGuardInterval_19_256)
    {
        return "19256";
    }
    return "auto";
}

QString SatIP::fec(DTVCodeRate fec)
{
    if (fec == DTVCodeRate::kFEC_1_2)
    {
        return "12";
    }
    if (fec == DTVCodeRate::kFEC_2_3)
    {
        return "23";
    }
    if (fec == DTVCodeRate::kFEC_3_4)
    {
        return "34";
    }
    if (fec == DTVCodeRate::kFEC_3_5)
    {
        return "35";
    }
    if (fec == DTVCodeRate::kFEC_4_5)
    {
        return "45";
    }
    if (fec == DTVCodeRate::kFEC_5_6)
    {
        return "56";
    }
    if (fec == DTVCodeRate::kFEC_6_7)
    {
        return "67";
    }
    if (fec == DTVCodeRate::kFEC_7_8)
    {
        return "78";
    }
    if (fec == DTVCodeRate::kFEC_8_9)
    {
        return "89";
    }
    if (fec == DTVCodeRate::kFEC_9_10)
    {
        return "910";
    }
    return "auto";
}

QString SatIP::ro(DTVRollOff ro)
{
    if (ro == DTVRollOff::kRollOff_35)
    {
        return "0.35";
    }
    if (ro == DTVRollOff::kRollOff_20)
    {
        return "0.20";
    }
    if (ro == DTVRollOff::kRollOff_25)
    {
        return "0.25";
    }
    if (ro == DTVRollOff::kRollOff_Auto)
    {
        return "auto";
    }
    return "auto";
}

QString SatIP::pol(DTVPolarity pol)
{
    if (pol == DTVPolarity::kPolarityVertical)
    {
        return "v";
    }
    if (pol == DTVPolarity::kPolarityHorizontal)
    {
        return "h";
    }
    if (pol == DTVPolarity::kPolarityRight)
    {
        return "r";
    }
    if (pol == DTVPolarity::kPolarityLeft)
    {
        return "l";
    }
    return "auto";
}

#include <unistd.h>

#include "dtvsignalmonitor.h"
#include "scanstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"
#include "dvbtables.h"

/** \class DTVSignalMonitor
 *  \brief This class is intended to detect the presence of needed tables.
 */

DTVSignalMonitor::DTVSignalMonitor(int capturecardnum,
                                   uint wait_for_mask,
                                   const char *name)
    : SignalMonitor(capturecardnum, wait_for_mask, name), stream_data(NULL),
      seenPAT(QObject::tr("Seen PAT"), "seen_pat", 1, true, 0, 1, 0),
      seenPMT(QObject::tr("Seen PMT"), "seen_pmt", 1, true, 0, 1, 0),
      seenMGT(QObject::tr("Seen MGT"), "seen_mgt", 1, true, 0, 1, 0),
      seenVCT(QObject::tr("Seen VCT"), "seen_vct", 1, true, 0, 1, 0),
      seenNIT(QObject::tr("Seen NIT"), "seen_nit", 1, true, 0, 1, 0),
      seenSDT(QObject::tr("Seen SDT"), "seen_sdt", 1, true, 0, 1, 0),
      matchingPAT(QObject::tr("Matching PAT"),
                  "matching_pat", 1, true, 0, 1, 0),
      matchingPMT(QObject::tr("Matching PMT"),
                  "matching_pmt", 1, true, 0, 1, 0),
      matchingMGT(QObject::tr("Matching MGT"),
                  "matching_mgt", 1, true, 0, 1, 0),
      matchingVCT(QObject::tr("Matching VCT"),
                  "matching_vct", 1, true, 0, 1, 0),
      matchingNIT(QObject::tr("Matching NIT"),
                  "matching_nit", 1, true, 0, 1, 0),
      matchingSDT(QObject::tr("Matching SDT"),
                  "matching_sdt", 1, true, 0, 1, 0),
      majorChannel(-1), minorChannel(-1), programNumber(-1),
      error("")
{
}

QStringList DTVSignalMonitor::GetStatusList(bool kick)
{
    QStringList list = SignalMonitor::GetStatusList(kick);
    statusLock.lock();
    // mpeg tables
    if (flags & kDTVSigMon_WaitForPAT)
    {
        list<<seenPAT.GetName()<<seenPAT.GetStatus();
        list<<matchingPAT.GetName()<<matchingPAT.GetStatus();
    }
    if (flags & kDTVSigMon_WaitForPMT)
    {
        list<<seenPMT.GetName()<<seenPMT.GetStatus();
        list<<matchingPMT.GetName()<<matchingPMT.GetStatus();
    }
    // atsc tables
    if (flags & kDTVSigMon_WaitForMGT)
    {
        list<<seenMGT.GetName()<<seenMGT.GetStatus();
        list<<matchingMGT.GetName()<<matchingMGT.GetStatus();
    }
    if (flags & kDTVSigMon_WaitForVCT)
    {
        list<<seenVCT.GetName()<<seenVCT.GetStatus();
        list<<matchingVCT.GetName()<<matchingVCT.GetStatus();
    }
    // dvb tables
    if (flags & kDTVSigMon_WaitForNIT)
    {
        list<<seenNIT.GetName()<<seenNIT.GetStatus();
        list<<matchingNIT.GetName()<<matchingNIT.GetStatus();
    }
    if (flags & kDTVSigMon_WaitForSDT)
    {
        list<<seenSDT.GetName()<<seenSDT.GetStatus();
        list<<matchingSDT.GetName()<<matchingSDT.GetStatus();
    }
    if (error != "")
    {
        list<<"error"<<error;
    }
    statusLock.unlock();
    return list;
}

void DTVSignalMonitor::AddFlags(uint _flags)
{
    SignalMonitor::AddFlags(_flags);
    UpdateMonitorValues();
}

void DTVSignalMonitor::RemoveFlags(uint _flags)
{
    SignalMonitor::RemoveFlags(_flags);
    UpdateMonitorValues();
}

void DTVSignalMonitor::UpdateMonitorValues()
{
    statusLock.lock();
    seenPAT.SetValue(    (flags & kDTVSigMon_PATSeen)  ? 1 : 0);
    seenPMT.SetValue(    (flags & kDTVSigMon_PMTSeen)  ? 1 : 0);
    seenMGT.SetValue(    (flags & kDTVSigMon_MGTSeen)  ? 1 : 0);
    seenVCT.SetValue(    (flags & kDTVSigMon_VCTSeen)  ? 1 : 0);
    seenNIT.SetValue(    (flags & kDTVSigMon_NITSeen)  ? 1 : 0);
    seenSDT.SetValue(    (flags & kDTVSigMon_SDTSeen)  ? 1 : 0);
    matchingPAT.SetValue((flags & kDTVSigMon_PATMatch) ? 1 : 0);
    matchingPMT.SetValue((flags & kDTVSigMon_PMTMatch) ? 1 : 0);
    matchingMGT.SetValue((flags & kDTVSigMon_MGTMatch) ? 1 : 0);
    matchingVCT.SetValue((flags & kDTVSigMon_VCTMatch) ? 1 : 0);
    matchingNIT.SetValue((flags & kDTVSigMon_NITMatch) ? 1 : 0);
    matchingSDT.SetValue((flags & kDTVSigMon_SDTMatch) ? 1 : 0);
    statusLock.unlock();
}

void DTVSignalMonitor::SetChannel(int major, int minor)
{
    VERBOSE(VB_CHANNEL, QString("DTVSignalMonitor::SetChannel(%1, %2)")
            .arg(major).arg(minor));
    if (GetATSCStreamData() && (majorChannel != major || minorChannel != minor))
    {
        RemoveFlags(kDTVSigMon_PATSeen | kDTVSigMon_PATMatch |
                    kDTVSigMon_PMTSeen | kDTVSigMon_PMTMatch |
                    kDTVSigMon_VCTSeen | kDTVSigMon_VCTMatch);
        majorChannel = major;
        minorChannel = minor;
        GetATSCStreamData()->SetDesiredChannel(major, minor);
        AddFlags(kDTVSigMon_WaitForVCT | kDTVSigMon_WaitForPAT);
    }
}

void DTVSignalMonitor::SetProgramNumber(int progNum)
{
    VERBOSE(VB_CHANNEL, QString("DTVSignalMonitor::SetProgramNumber(): %1")
            .arg(progNum));
    if (programNumber != progNum)
    {
        RemoveFlags(kDTVSigMon_PMTSeen | kDTVSigMon_PMTMatch);
        programNumber = progNum;
        if (GetStreamData())
            GetStreamData()->SetDesiredProgram(programNumber);
        AddFlags(kDTVSigMon_WaitForPMT);
    }
}

void DTVSignalMonitor::SetStreamData(MPEGStreamData *data)
{
    stream_data = data;
    if (!data)
        return;

    connect(data, SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
            this, SLOT(SetPAT(const ProgramAssociationTable*)));
    
    connect(data, SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
            this, SLOT(SetPMT(uint, const ProgramMapTable*)));

    ATSCStreamData *atsc = GetATSCStreamData();
    if (atsc)
    {
        connect(atsc, SIGNAL(UpdateMGT(const MasterGuideTable*)),
                this, SLOT(SetMGT(const MasterGuideTable*)));

        connect(
            atsc,
            SIGNAL(UpdateTVCT(uint, const TerrestrialVirtualChannelTable*)),
            this, SLOT(SetVCT(uint, const TerrestrialVirtualChannelTable*)));

        connect(
            atsc, SIGNAL(UpdateCVCT(uint, const CableVirtualChannelTable*)),
            this, SLOT(  SetVCT(    uint, const CableVirtualChannelTable*)));
    }

    DVBStreamData *dvbc = GetDVBStreamData();
    if (dvbc)
    {
        connect(dvbc, SIGNAL(UpdateNIT(const NetworkInformationTable*)),
                this, SLOT(  SetNIT(   const NetworkInformationTable*)));

        connect(dvbc, SIGNAL(UpdateSDT(uint, const ServiceDescriptionTable*)),
                this, SLOT(  SetSDT(   uint, const ServiceDescriptionTable*)));
    }
}

void DTVSignalMonitor::SetPAT(const ProgramAssociationTable *pat)
{
    AddFlags(kDTVSigMon_PATSeen);
    int pmt_pid = pat->FindPID(programNumber);
    if (GetStreamData() && pmt_pid)
    {
        AddFlags(kDTVSigMon_PATMatch);
        GetStreamData()->AddListeningPID(pmt_pid);
    }
    else if (programNumber >= 0 && pat->ProgramCount() == 1)
    {
        QString errStr = QObject::tr("Program #%1 not found in PAT!").arg(programNumber);
        VERBOSE(VB_IMPORTANT, errStr<<endl<<pat->toString()<<endl);
        VERBOSE(VB_IMPORTANT, "But there is only one program in the PAT, "
                "so we'll just use it");
        SetProgramNumber(pat->ProgramNumber(0));
        AddFlags(kDTVSigMon_PATMatch);
        GetStreamData()->AddListeningPID(pat->ProgramPID(0));
    }
    else if (programNumber >= 0)
    {
        error = QObject::tr("Program #%1 not found in PAT!").arg(programNumber);
        VERBOSE(VB_IMPORTANT, error<<endl<<pat->toString()<<endl);
    }
}

void DTVSignalMonitor::SetPMT(uint, const ProgramMapTable *pmt)
{
    AddFlags(kDTVSigMon_PMTSeen);

    // if PMT contains audio and/or video stream set as matching.
    for (uint i = 0; i<pmt->StreamCount(); i++)
    {
        uint type = pmt->StreamType(i);
        if ((StreamID::MPEG2Video == type) || (StreamID::MPEG1Video == type) ||
            (StreamID::MPEG2Audio == type) || (StreamID::MPEG1Audio == type) ||
            (StreamID::AC3Audio   == type) || (StreamID::AACAudio   == type) ||
            (StreamID::DTSAudio   == type))
        {
            AddFlags(kDTVSigMon_PMTMatch);
            return;
        }
    }
}

void DTVSignalMonitor::SetMGT(const MasterGuideTable* mgt)
{
    AddFlags(kDTVSigMon_MGTSeen);

    if (!GetATSCStreamData())
        return;

    for (uint i=0; i<mgt->TableCount(); i++)
    {
        if ((TableClass::TVCTc == mgt->TableClass(i)) ||
            (TableClass::CVCTc == mgt->TableClass(i)))
        {
            GetATSCStreamData()->AddListeningPID(mgt->TablePID(i));
            AddFlags(kDTVSigMon_MGTMatch);
        }
    }
}

void DTVSignalMonitor::SetVCT(uint, const TerrestrialVirtualChannelTable* tvct)
{ 
    AddFlags(kDTVSigMon_VCTSeen | kDTVSigMon_TVCTSeen);
    int idx = tvct->Find(majorChannel, minorChannel);

    if (idx < 0)
        return;

    VERBOSE(VB_CHANNEL, QString(
                "DTVSignalMonitor::SetVCT(): tvct->ProgramNumber(%1): %2")
            .arg(idx).arg(tvct->ProgramNumber(idx)));

    SetProgramNumber(tvct->ProgramNumber(idx));
    AddFlags(kDTVSigMon_VCTMatch | kDTVSigMon_TVCTMatch);
}

void DTVSignalMonitor::SetVCT(uint, const CableVirtualChannelTable* cvct)
{
    AddFlags(kDTVSigMon_VCTSeen | kDTVSigMon_CVCTSeen);
    int idx = cvct->Find(majorChannel, minorChannel);

    if (idx < 0)
        return;

    VERBOSE(VB_CHANNEL, QString(
                "DTVSignalMonitor::SetVCT(): cvct->ProgramNumber(%1): %2")
            .arg(idx).arg(cvct->ProgramNumber(idx)));

    SetProgramNumber(cvct->ProgramNumber(idx));
    AddFlags(kDTVSigMon_VCTMatch | kDTVSigMon_CVCTMatch);
}

void DTVSignalMonitor::SetNIT(const NetworkInformationTable *nit)
{
    VERBOSE(VB_CHANNEL, QString("DTVSignalMonitor::SetNIT(): net_id = %1")
            .arg(nit->NetworkID()));
    AddFlags(kDTVSigMon_NITSeen);
    if (!GetDVBStreamData())
        return;
}

void DTVSignalMonitor::SetSDT(uint, const ServiceDescriptionTable *sdt)
{
    VERBOSE(VB_CHANNEL, QString("DTVSignalMonitor::SetSDT(): "
                                "tsid = %1 orig_net_id = %2")
            .arg(sdt->TSID()).arg(sdt->OriginalNetworkID()));
    AddFlags(kDTVSigMon_SDTSeen);
    if (!GetDVBStreamData())
        return;
}

ATSCStreamData *DTVSignalMonitor::GetATSCStreamData()
{
    return dynamic_cast<ATSCStreamData*>(stream_data);
}

DVBStreamData *DTVSignalMonitor::GetDVBStreamData()
{
    if (GetScanStreamData())
        return &(GetScanStreamData()->dvb);
    return dynamic_cast<DVBStreamData*>(stream_data);
}

ScanStreamData *DTVSignalMonitor::GetScanStreamData()
{
    return dynamic_cast<ScanStreamData*>(stream_data);
}

const ATSCStreamData *DTVSignalMonitor::GetATSCStreamData() const
{
    return dynamic_cast<const ATSCStreamData*>(stream_data);
}

const DVBStreamData *DTVSignalMonitor::GetDVBStreamData() const
{
    if (GetScanStreamData())
        return &(GetScanStreamData()->dvb);
    return dynamic_cast<const DVBStreamData*>(stream_data);
}

const ScanStreamData *DTVSignalMonitor::GetScanStreamData() const
{
    return dynamic_cast<const ScanStreamData*>(stream_data);
}

/** \fn  SignalMonitor::WaitForLock(int)
 *  \brief Wait for a StatusSignaLock(int) of true.
 *
 *   This can be called only after the signal
 *   monitoring thread has been started.
 *
 *  \param timeout maximum time to wait in milliseconds.
 *  \return true if signal was acquired.
 */
bool DTVSignalMonitor::WaitForLock(int timeout)
{
    statusLock.lock();
    if (-1 == timeout)
        timeout = signalLock.GetTimeout();
    statusLock.unlock();
    if (timeout < 0)
        return false;

    QTime t;
    t.start();
    while (t.elapsed()<timeout && running)
    {
        SignalMonitorList slist = 
            SignalMonitorValue::Parse(GetStatusList());
        if (SignalMonitorValue::AllGood(slist))
            return true;
        usleep(250);
    }

    return false;
}

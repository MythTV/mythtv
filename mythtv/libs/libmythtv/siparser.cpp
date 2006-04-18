// Std C++ headers
#include <algorithm>
#include <cmath>
using namespace std;

// Qt headers
#include <qdatetime.h>
#include <qregexp.h>
#include <qptrvector.h>

// MythTV headers
#include "mythcontext.h"
#include "mythdbcon.h"
#include "util.h"
#include "iso639.h"
#include "atsc_huffman.h"
#include "pespacket.h"
#include "atsctables.h"
#include "atscstreamdata.h"
#include "dvbstreamdata.h"
#include "dvbtables.h"
#include "dvbrecorder.h"
#include "eithelper.h"

// MythTV DVB headers
#include "siparser.h"

#define LOC QString("SIParser: ")
#define LOC_ERR QString("SIParser, Error: ")

/** \class SIParser
 *  This class parses DVB SI and ATSC PSIP tables.
 *
 *  This class is generalized so it can be used with DVB Cards with a simple
 *  sct filter, sending the read data into this class, and the PCHDTV card by
 *  filtering the TS packets through another class to convert it into tables,
 *  and passing this data into this class as well.
 *
 *  Both DVB and ATSC are combined into this class since ATSC over DVB is 
 *  present in some place.  (One example is PBS on AMC3 in North America).
 *  Argentenia has also has announced their Digital TV Standard will be 
 *  ATSC over DVB-T
 *
 *  Implementation of OpenCable or other MPEG-TS based standards (DirecTV?)
 *  is also possible with this class if their specs are ever known.
 *
 */
SIParser::SIParser(const char *name) :
    QObject(NULL, name),
    // Common Variables
    table_standard(SI_STANDARD_AUTO),
    // ATSC Storage
    atsc_stream_data(NULL),         dvb_stream_data(NULL),
    // Mutex Locks
    pmap_lock(true),
    // State variables
    ThreadRunning(false),           exitParserThread(false),
    // EIT variables
    eit_reset(false),               eit_rate(1.0f),
    eit_dn_long(false),             eit_helper(NULL)
{
    SetATSCStreamData(new ATSCStreamData(-1,-1));
    SetDVBStreamData(new DVBStreamData());

    /* Initialize the Table Handlers */
    Table[SERVICES] = new ServiceHandler();
}

SIParser::~SIParser()
{
    QMutexLocker locker(&pmap_lock);
    for (uint i = 0; i < NumHandlers; ++i)
    {
        if (Table[i])
            delete Table[i];
    }
}

void SIParser::deleteLater(void)
{
    QMutexLocker locker(&pmap_lock);
    disconnect(); // disconnect signals we may be sending...
    QObject::deleteLater();
}

/* Resets all trackers, and closes all section filters */
void SIParser::Reset(void)
{
    VERBOSE(VB_SIPARSER, LOC + "About to do a reset");
    QMutexLocker locker(&pmap_lock);

    VERBOSE(VB_SIPARSER, LOC + "Closing all PIDs");
    DelAllPids();
    atsc_eit_inuse_pid.clear();
    atsc_ett_inuse_pid.clear();
    atsc_eit_pid.clear();
    atsc_ett_pid.clear();
    sourceid_to_channel.clear();

    VERBOSE(VB_SIPARSER, LOC + "Resetting all Table Handlers");

    for (int x = 0; x < NumHandlers ; x++)
        Table[x]->Reset();

    atsc_stream_data->Reset(-1,-1);
    dvb_stream_data->Reset();

    VERBOSE(VB_SIPARSER, LOC + "SIParser Reset due to channel change");
}

void SIParser::CheckTrackers(void)
{
    QMutexLocker locker(&pmap_lock);

    uint16_t pid;
    uint8_t filter,mask;

    /* Check Dependencies and update if necessary */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->Complete())
        {
            VERBOSE(VB_SIPARSER, LOC + QString("Table[%1]->Complete() == true")
                    .arg((tabletypes) x));
            for (int y = 0 ; y < NumHandlers ; y++)
                Table[y]->DependencyMet((tabletypes) x);
// TODO: Emit completion here for tables to siscan
        }
    }

    /* Handle opening any PIDs in this loop only */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->RequirePIDs())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Table[%1]->RequirePIDs() == true")
                    .arg(tabletypes2string[x]));
            while (Table[x]->GetPIDs(pid,filter,mask))
            {
                AddPid(pid, mask, filter, true, 10/*bufferFactor*/);
            }
        }
    }
    // for MPEG PAT
    AddPid(MPEG_PAT_PID, 0xFF, TableID::PAT, true, 10 /*bufferFactor*/);
    // for MPEG PMT
    if (!pnum_pid.empty())
    {
        uint pnum = 0xFFFFFFFF;
        if (SI_STANDARD_ATSC == table_standard)
            pnum = atsc_stream_data->DesiredProgram();
        if (SI_STANDARD_DVB == table_standard)
            pnum = dvb_stream_data->DesiredProgram();

        pnum_pid_map_t::const_iterator it = pnum_pid.begin();
        for (; it != pnum_pid.end(); ++it)
        {
            if (it.key() == pnum)
                AddPid(*it, 0xFF, TableID::PMT, true, 10 /*bufferFactor*/);
        }
        //pnum_pid.clear();
    }
    // for DVB NIT
    if (SI_STANDARD_DVB == table_standard)
    {
        AddPid(DVB_NIT_PID, 0xFF, TableID::NIT, true, 10 /*bufferFactor*/);
    }

    // for ATSC STT and MGT.
    if (SI_STANDARD_ATSC == table_standard)
        AddPid(ATSC_PSIP_PID, 0x00, 0xFF, true, 10 /*bufferFactor*/);

    if (SI_STANDARD_DVB == table_standard)
    {
        if (dvb_stream_data->HasAllNITSections())
        {
            Table[SERVICES]->DependencyMet(NETWORK);
        }
    }

    AdjustEITPids();
}

/** \fn SIParser::AdjustEITPids(void)
 *  \brief Adjusts EIT PID monitoring to monitor the right number of EIT PIDs.
 *
 *   NOTE: Must be called with a pmap lock
 */
void SIParser::AdjustEITPids(void)
{
    if (SI_STANDARD_ATSC == table_standard)
    {
        uint i, eit_count = (uint) round(atsc_eit_pid.size() * eit_rate);
        if (atsc_eit_inuse_pid.size() == eit_count && !eit_reset)
            return;

        eit_reset = false;

        // delete old pids
        for (i = 0; i < atsc_eit_inuse_pid.size(); i++)
            DelPid(atsc_eit_inuse_pid[i]);
        for (i = 0; i < atsc_ett_inuse_pid.size(); i++)
            DelPid(atsc_ett_inuse_pid[i]);
        atsc_eit_inuse_pid.clear();
        atsc_ett_inuse_pid.clear();

        if (sourceid_to_channel.empty())
            return;

        // add new pids
        uint bufferFactor = 10;
        atsc_eit_pid_map_t::const_iterator it = atsc_eit_pid.begin();
        for (i = 0; it != atsc_eit_pid.end() && (i < eit_count); (++it),(i++))
        {
            VERBOSE(VB_SIPARSER, LOC + QString("EIT-%1 on pid(0x%2)")
                    .arg(it.key(),2).arg(*it, 0, 16));
            atsc_eit_inuse_pid[it.key()] = *it;
            AddPid(*it, 0xFF, TableID::EIT, true, bufferFactor);
        }

        uint ett_count = (uint) round(atsc_ett_pid.size() * eit_rate);
        atsc_ett_pid_map_t::const_iterator it2 = atsc_ett_pid.begin();
        for (i = 0; it2 != atsc_ett_pid.end() && (i < ett_count);(++it2),(i++))
        {
            VERBOSE(VB_SIPARSER, LOC + QString("ETT-%1 on pid(0x%2)")
                    .arg(it2.key(),2).arg(*it2, 0, 16));
            atsc_ett_inuse_pid[it2.key()] = *it2;
            AddPid(*it2, 0xFF, TableID::ETT, true, bufferFactor);
        }
    }
    else if (SI_STANDARD_DVB == table_standard)
    {
        uint bufferFactor = 1000;
        if (eit_rate >= 0.5f && !dvb_srv_collect_eit.empty())
        {
            AddPid(DVB_EIT_PID, 0x00, 0xFF, true, bufferFactor);
            if (eit_dn_long)
                AddPid(DVB_DNLONG_EIT_PID, 0x00, 0xFF, true, bufferFactor);
        }
        else
        {
            DelPid(DVB_EIT_PID);
            if (eit_dn_long)
                DelPid(DVB_DNLONG_EIT_PID);
        }
    }    
}


void SIParser::SetATSCStreamData(ATSCStreamData *stream_data)
{
    VERBOSE(VB_IMPORTANT, LOC + "Setting ATSCStreamData");
    QMutexLocker locker(&pmap_lock);

    if (stream_data == atsc_stream_data)
        return;

    ATSCStreamData *old_data = atsc_stream_data;
    atsc_stream_data = stream_data;

    if (old_data && ((void*)old_data == (void*)dvb_stream_data))
        dvb_stream_data = (DVBStreamData*) stream_data;

    if (old_data)
        delete old_data;

    // Use any cached tables
    if (atsc_stream_data->HasCachedMGT())
    {
        const MasterGuideTable *mgt = atsc_stream_data->GetCachedMGT();
        HandleMGT(mgt);
        atsc_stream_data->ReturnCachedTable(mgt);
    }

    tvct_vec_t tvcts = atsc_stream_data->GetAllCachedTVCTs();
    for (uint i = 0; i < tvcts.size(); i++)
        HandleVCT(0, tvcts[i]);
    atsc_stream_data->ReturnCachedTVCTTables(tvcts);

    cvct_vec_t cvcts = atsc_stream_data->GetAllCachedCVCTs();
    for (uint i = 0; i < cvcts.size(); i++)
        HandleVCT(0, cvcts[i]);
    atsc_stream_data->ReturnCachedCVCTTables(cvcts);

    pmt_map_t pmts = atsc_stream_data->GetCachedPMTMap();
    pmt_map_t::const_iterator it = pmts.begin();
    for (; it != pmts.end(); ++it)
    {
        pmt_vec_t::const_iterator vit = (*it).begin();
        for (; vit != (*it).end(); ++vit)
            HandlePMT(it.key(), *vit);
    }
    atsc_stream_data->ReturnCachedTables(pmts);

    // MPEG table signals
    connect(atsc_stream_data,
            SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
            this,
            SLOT(  HandlePAT(const ProgramAssociationTable*)));
    connect(atsc_stream_data,
            SIGNAL(UpdateCAT(const ConditionalAccessTable*)),
            this,
            SLOT(  HandleCAT(const ConditionalAccessTable*)));
    connect(atsc_stream_data,
            SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
            this,
            SLOT(  HandlePMT(uint, const ProgramMapTable*)));

    // ATSC table signals
    connect(atsc_stream_data, SIGNAL(UpdateMGT(const MasterGuideTable*)),
            this,             SLOT(  HandleMGT(const MasterGuideTable*)));
    connect(atsc_stream_data,
            SIGNAL(UpdateVCT(uint, const VirtualChannelTable*)),
            this,
            SLOT(  HandleVCT(uint, const VirtualChannelTable*)));

    connect(atsc_stream_data, SIGNAL(UpdateSTT(const SystemTimeTable*)),
            this,             SLOT(  HandleSTT(const SystemTimeTable*)));

    // EIT table signals
    connect(atsc_stream_data,
            SIGNAL(UpdateEIT(uint, const EventInformationTable*)),
            this,
            SLOT(  HandleEIT(uint, const EventInformationTable*)));
    connect(atsc_stream_data,
            SIGNAL(UpdateETT(uint, const ExtendedTextTable*)),
            this,
            SLOT(  HandleETT(uint, const ExtendedTextTable*)));
}

void SIParser::SetDVBStreamData(DVBStreamData *stream_data)
{
    VERBOSE(VB_IMPORTANT, LOC + "Setting DVBStreamData");
    QMutexLocker locker(&pmap_lock);

    if (stream_data == dvb_stream_data)
        return;

    DVBStreamData *old_data = dvb_stream_data;
    dvb_stream_data = stream_data;

    if (old_data && ((void*)old_data == (void*)atsc_stream_data))
        atsc_stream_data = (ATSCStreamData*) stream_data;

    if (old_data)
        delete old_data;

    // Use any cached tables
    pmt_map_t pmts = atsc_stream_data->GetCachedPMTMap();
    pmt_map_t::const_iterator it = pmts.begin();
    for (; it != pmts.end(); ++it)
    {
        pmt_vec_t::const_iterator vit = (*it).begin();
        for (; vit != (*it).end(); ++vit)
            HandlePMT(it.key(), *vit);
    }
    atsc_stream_data->ReturnCachedTables(pmts);

    // MPEG table signals
    connect(dvb_stream_data,
            SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
            this,
            SLOT(  HandlePAT(const ProgramAssociationTable*)));
    connect(dvb_stream_data,
            SIGNAL(UpdateCAT(const ConditionalAccessTable*)),
            this,
            SLOT(  HandleCAT(const ConditionalAccessTable*)));
    connect(dvb_stream_data,
            SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
            this,
            SLOT(  HandlePMT(uint, const ProgramMapTable*)));

    // DVB table signals
    connect(dvb_stream_data,
            SIGNAL(UpdateNIT(const NetworkInformationTable*)),
            this,
            SLOT(  HandleNIT(const NetworkInformationTable*)));

    // EIT table signal
    connect(dvb_stream_data,
            SIGNAL(UpdateEIT(const DVBEventInformationTable*)),
            this,
            SLOT(  HandleEIT(const DVBEventInformationTable*)));
}

void SIParser::SetStreamData(MPEGStreamData *stream_data)
{
    VERBOSE(VB_IMPORTANT, LOC + "SetStreamData("<<stream_data<<")");

    ATSCStreamData *atsc_sd = dynamic_cast<ATSCStreamData*>(stream_data);
    if (atsc_sd && (atsc_sd != atsc_stream_data))
        SetATSCStreamData(atsc_sd);

    DVBStreamData *dvb_sd = dynamic_cast<DVBStreamData*>(stream_data);
    if (dvb_sd && (dvb_sd != dvb_stream_data))
        SetDVBStreamData(dvb_sd);
}

/** \fn SIParser::SetTableStandard(const QString &)
 *  \brief Adds basic PID's corresponding to standard to the list
 *         of PIDs we are listening to.
 *
 *   For ATSC this adds the PAT (0x0) and PSIP (0x1ffb) pids.
 *
 *   For DVB this adds the PAT(0x0), SDT (0x11), and NIT (0x10) pids.
 *
 *   Note: This actually adds all of the above so as to simplify channel
 *         scanning, but this may change as this can break ATSC.
 */
void SIParser::SetTableStandard(const QString &table_std)
{
    VERBOSE(VB_SIPARSER, LOC + QString("SetTableStandard(%1)").arg(table_std));
    bool is_atsc = table_std.lower() == "atsc";

    QMutexLocker locker(&pmap_lock);

    table_standard = (is_atsc) ? SI_STANDARD_ATSC : SI_STANDARD_DVB;

    Table[SERVICES]->Request(0);

    for (int x = 0 ; x < NumHandlers ; x++)
        Table[x]->SetSIStandard((SISTANDARD)table_standard);
}

/** \fn SIParser::SetDesiredProgram(uint)
 */
void SIParser::SetDesiredProgram(uint mpeg_program_number, bool reset)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("SetDesiredProgram(%1, %2)")
            .arg(mpeg_program_number).arg((reset) ? "reset" : "no reset"));

    QMutexLocker locker(&pmap_lock);

    if (reset)
    {
        atsc_stream_data->Reset(-1, -1);
        ((MPEGStreamData*)atsc_stream_data)->Reset(mpeg_program_number);
    }
    atsc_stream_data->AddListeningPID(ATSC_PSIP_PID);

    if (reset)
    {
        dvb_stream_data->Reset();
        ((MPEGStreamData*)dvb_stream_data)->Reset(mpeg_program_number);
    }
    dvb_stream_data->AddListeningPID(DVB_NIT_PID);
    dvb_stream_data->AddListeningPID(DVB_SDT_PID);

    if (reset)
        return;

    // Set just the desired program, without a reset
    atsc_stream_data->SetDesiredProgram(mpeg_program_number);
    dvb_stream_data->SetDesiredProgram(mpeg_program_number);

    // Get needed tables from cache, currently only for atsc_stream_data
    pat_ptr_t pat = atsc_stream_data->GetCachedPAT();
    pmt_ptr_t pmt = atsc_stream_data->GetCachedPMT(mpeg_program_number, 0);

    if (pat)
    {
        HandlePAT(pat);
        atsc_stream_data->ReturnCachedTable(pat);
    }

    if (pmt)
    {
        HandlePMT(mpeg_program_number, pmt);
        atsc_stream_data->ReturnCachedTable(pmt);
    }
}

/** \fn SIParser::ReinitSIParser(const QString&, MPEGStreamData*, uint)
 *  \brief Convenience function that calls SetTableStandard(const QString &)
 *         and SetDesiredProgram(uint).
 */
void SIParser::ReinitSIParser(const QString  &si_std,
                              MPEGStreamData *stream_data,
                              uint            mpeg_program_number)
{
    VERBOSE(VB_SIPARSER, LOC + QString("ReinitSIParser(std %1, %2 #%3)")
            .arg(si_std)
            .arg((si_std.lower() == "atsc") ? "program" : "service")
            .arg(mpeg_program_number));

    QMutexLocker locker(&pmap_lock);

    SetTableStandard(si_std);
    SetStreamData(stream_data);
    SetDesiredProgram(mpeg_program_number, !stream_data);
}

/*------------------------------------------------------------------------
 *   COMMON PARSER CODE
 *------------------------------------------------------------------------*/

void SIParser::ParseTable(uint8_t *buffer, int /*size*/, uint16_t pid)
{
    QMutexLocker locker(&pmap_lock);

    const PESPacket pes = PESPacket::ViewData(buffer);
    const PSIPTable psip(pes);

    if (!psip.SectionSyntaxIndicator())
    {
        VERBOSE(VB_SIPARSER, LOC + "section_syntax_indicator == 0: " +
                QString("Ignoring table 0x%1").arg(psip.TableID(),0,16));
        return;
    }

    // In Detection mode determine what table_standard you are using.
    if (table_standard == SI_STANDARD_AUTO)
    {
        if (TableID::MGT == psip.TableID()  ||
            TableID::TVCT == psip.TableID() ||
            TableID::CVCT == psip.TableID())
        {
            table_standard = SI_STANDARD_ATSC;
            VERBOSE(VB_SIPARSER, LOC + "Table Standard Detected: ATSC");
        }
        else if (TableID::SDT  == psip.TableID() ||
                 TableID::SDTo == psip.TableID())
        {
            table_standard = SI_STANDARD_DVB;
            VERBOSE(VB_SIPARSER, LOC + "Table Standard Detected: DVB");
        }
    }

    // Parse tables for ATSC
    if (SI_STANDARD_ATSC == table_standard)
    {
        atsc_stream_data->HandleTables(pid, psip);
        return;
    }

    // Parse tables for DVB
    dvb_stream_data->HandleTables(pid, psip);

    // Parse DVB specific SDT tables
    if (TableID::SDT  == psip.TableID() || TableID::SDTo == psip.TableID())
    {
        // Service Tables
        ServiceDescriptionTable sdt(psip);

        uint sect_tsid = (TableID::SDT == sdt.TableID()) ? 0 : sdt.TSID();

        tablehead_t head;
        head.table_id       = buffer[0];
        head.section_length = ((buffer[1] & 0x0f)<<8) | buffer[2];
        head.table_id_ext   = (buffer[3] << 8) | buffer[4];
        head.current_next   = (buffer[5] & 0x1);
        head.version        = ((buffer[5] >> 1) & 0x1f);
        head.section_number = buffer[6];
        head.section_last   = buffer[7];

        if (!Table[SERVICES]->AddSection(&head, sect_tsid, 0))
            HandleSDT(sect_tsid, &sdt);
    }
}

/*------------------------------------------------------------------------
 * MPEG Table handlers
 *------------------------------------------------------------------------*/

void SIParser::HandlePAT(const ProgramAssociationTable *pat)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("PAT Version: %1  Tuned to TransportID: %2")
            .arg(pat->Version()).arg(pat->TransportStreamID()));

    pnum_pid.clear();
    for (uint i = 0; i < pat->ProgramCount(); ++i)
    {
        // DVB Program 0 in the PAT represents the location of the NIT
        if (0 == pat->ProgramNumber(i) && SI_STANDARD_ATSC != table_standard)
        {
            VERBOSE(VB_SIPARSER, LOC + "NIT Present on this transport " +
                    QString(" on PID 0x%1").arg(pat->ProgramPID(i),0,16));
            continue;
        }

        pnum_pid[pat->ProgramNumber(i)] = pat->ProgramPID(i);
    }
}

void SIParser::HandleCAT(const ConditionalAccessTable *cat)
{
    const desc_list_t list = MPEGDescriptor::ParseOnlyInclude(
        cat->Descriptors(), cat->DescriptorsLength(),
        DescriptorID::conditional_access);

    for (uint i = 0; i < list.size(); i++)
    {
        ConditionalAccessDescriptor ca(list[i]);
        VERBOSE(VB_GENERAL, LOC + QString("CA System 0x%1, EMM PID = 0x%2")
                .arg(ca.SystemID(),0,16).arg(ca.PID()));
    }
}

// TODO: Catch serviceMove descriptor and send a signal when you get one
//       to retune to correct transport or send an error tuning the channel
void SIParser::HandlePMT(uint pnum, const ProgramMapTable *pmt)
{
    VERBOSE(VB_SIPARSER, LOC + QString(
                "PMT pn(%1) version(%2) cnt(%3) pid(0x%4)")
            .arg(pnum).arg(pmt->Version()).arg(pmt->StreamCount())
            .arg(pnum_pid[pmt->ProgramNumber()],0,16));

    if (SI_STANDARD_ATSC == table_standard)
    {
        if ((int)pmt->ProgramNumber() == atsc_stream_data->DesiredProgram())
            emit UpdatePMT(pnum_pid[pmt->ProgramNumber()], pmt);
    }
    if (SI_STANDARD_DVB == table_standard)
    {
        if ((int)pmt->ProgramNumber() == dvb_stream_data->DesiredProgram())
            emit UpdatePMT(pnum_pid[pmt->ProgramNumber()], pmt);
    }
}

/*------------------------------------------------------------------------
 * DVB Table handlers
 *------------------------------------------------------------------------*/

// Parse Network Descriptors (Name, Linkage)
void SIParser::HandleNITDesc(const desc_list_t &dlist)
{
    NetworkObject n;
    bool n_set = false;
    const unsigned char *d = NULL;

    d = MPEGDescriptor::Find(dlist, DescriptorID::network_name);
    if (d)
    {
        n_set = true;
        n.NetworkName = NetworkNameDescriptor(d).Name();
    }

    d = MPEGDescriptor::Find(dlist, DescriptorID::linkage);
    if (d)
    {
        n_set = true;
        const LinkageDescriptor linkage(d);
        n.LinkageTransportID = linkage.TSID();
        n.LinkageNetworkID   = linkage.OriginalNetworkID();
        n.LinkageServiceID   = linkage.ServiceID();
        n.LinkageType        = linkage.LinkageType();
        n.LinkagePresent     = 1;
#if 0
        // See ticket #778.
        // Breaks "DVB-S in Germany with Astra 19.2E"
        if (LinkageDescriptor::lt_TSContainingCompleteNetworkBouquetSI ==
            linkage.LinkageType())
        {
            PrivateTypes.GuideOnSingleTransport = true;
            PrivateTypes.GuideTransportID = n.LinkageTransportID;
        }
#endif
    }

    if (n_set)
        NITList.Network += n;
}

void SIParser::HandleNITTransportDesc(const desc_list_t &dlist,
                                      TransportObject   &tobj,
                                      QMap_uint16_t     &clist)
{
    for (uint i = 0; i < dlist.size(); i++)
    {
        if (DescriptorID::cable_delivery_system == dlist[i][0])
        {
            CableDeliverySystemDescriptor cdsd(dlist[i]);
            tobj.Type             = "DVB-C";
            tobj.Frequency        = cdsd.FrequencyHz();
            tobj.FEC_Outer        = cdsd.FECOuterString();
            tobj.Modulation       = cdsd.ModulationString();
            tobj.SymbolRate       = cdsd.SymbolRateHz();
            tobj.FEC_Inner        = cdsd.FECInnerString();
        }
        else if (DescriptorID::terrestrial_delivery_system == dlist[i][0])
        {
            TerrestrialDeliverySystemDescriptor tdsd(dlist[i]);
            tobj.Type             = "DVB-T";
            tobj.Frequency        = tdsd.FrequencyHz();
            tobj.Bandwidth        = tdsd.BandwidthString();
            tobj.Constellation    = tdsd.ConstellationString();
            tobj.Hiearchy         = tdsd.HierarchyString();
            tobj.CodeRateHP       = tdsd.CodeRateHPString();
            tobj.CodeRateLP       = tdsd.CodeRateLPString();
            tobj.GuardInterval    = tdsd.GuardIntervalString();
            tobj.TransmissionMode = tdsd.TransmissionModeString();
        }
        else if (DescriptorID::satellite_delivery_system == dlist[i][0])
        {
            SatelliteDeliverySystemDescriptor sdsd(dlist[i]);
            tobj.Type             = "DVB-S";
            tobj.Frequency        = sdsd.FrequencyHz();
            tobj.OrbitalLocation  = sdsd.OrbitalPositionString();
            tobj.Polarity         = sdsd.PolarizationString();
            tobj.Modulation       = sdsd.ModulationString();
            tobj.SymbolRate       = sdsd.SymbolRateHz();
            tobj.FEC_Inner        = sdsd.FECInnerString();
        }
        else if (DescriptorID::frequency_list == dlist[i][0])
        {
            FrequencyListDescriptor fld(dlist[i]);
            for (uint i = 0; i < fld.FrequencyCount(); i++)
                tobj.frequencies.push_back(fld.FrequencyHz(i));
        }
        else if (DescriptorID::dvb_uk_channel_list == dlist[i][0])
        {
            UKChannelListDescriptor ucld(dlist[i]);
            for (uint i = 0; i < ucld.ChannelCount(); i++)
                clist[ucld.ServiceID(i)] = ucld.ChannelNumber(i);
        }
    }
}

void SIParser::HandleNIT(const NetworkInformationTable *nit)
{
    ServiceHandler *sh = (ServiceHandler*) Table[SERVICES];

    const desc_list_t dlist = MPEGDescriptor::Parse(
        nit->NetworkDescriptors(), nit->NetworkDescriptorsLength());
    HandleNITDesc(dlist);

    TransportObject t;
    for (uint i = 0; i < nit->TransportStreamCount(); i++)
    {
        const desc_list_t dlist = MPEGDescriptor::Parse(
            nit->TransportDescriptors(i), nit->TransportDescriptorsLength(i));

        QMap_uint16_t chanNums;
        HandleNITTransportDesc(dlist, t, chanNums);

        t.TransportID = nit->TSID(i);
        t.NetworkID   = nit->OriginalNetworkID(i);

        sh->Request(nit->TSID(i));
    
        if (!chanNums.empty())
        {
            QMap_SDTObject &slist = sh->Services[nit->TSID(i)];
            QMap_uint16_t::const_iterator it = chanNums.begin();
            for (; it != chanNums.end(); ++it)
                slist[it.key()].ChanNum = it.data();
        }

        NITList.Transport += t;
    }
}

void SIParser::HandleSDT(uint /*tsid*/, const ServiceDescriptionTable *sdt)
{
    VERBOSE(VB_SIPARSER, LOC + QString("SDT: NetworkID=%1 TransportID=%2")
            .arg(sdt->OriginalNetworkID()).arg(sdt->TSID()));

    ServiceHandler *sh    = (ServiceHandler*) Table[SERVICES];
    QMap_SDTObject &slist = sh->Services[sdt->TSID()];

    bool cur = TableID::SDT == sdt->TableID();

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        SDTObject s;
        s.Version       = sdt->Version();
        s.TransportID   = sdt->TSID();
        s.NetworkID     = sdt->OriginalNetworkID();

        s.ServiceID     = sdt->ServiceID(i);
        bool has_eit    = (sdt->HasEITSchedule(i) ||
                           sdt->HasEITPresentFollowing(i));
        s.EITPresent    = (has_eit) ? 1 : 0;
        s.RunningStatus = sdt->RunningStatus(i);
        s.CAStatus      = sdt->IsEncrypted(i) ? 1 : 0;

        // Rename channel with info from DB
        if (slist.contains(sdt->ServiceID(i)))
            s.ChanNum = slist[sdt->ServiceID(i)].ChanNum;

        const desc_list_t list = MPEGDescriptor::ParseOnlyInclude(
            sdt->ServiceDescriptors(i), sdt->ServiceDescriptorsLength(i),
            DescriptorID::service);

        for (uint j = 0; j < list.size(); j++)
        {
            ServiceDescriptor sd(list[j]);
            s.ServiceType  = sd.ServiceType();
            s.ProviderName = sd.ServiceProviderName();
            s.ServiceName  = sd.ServiceName();
        }

        if (has_eit)
            dvb_srv_collect_eit[sdt->ServiceID(i)] = true;

        uint sect_tsid = (cur) ? 0 : sdt->TSID();
        sh->Services[sect_tsid][sdt->ServiceID(i)] = s;

        VERBOSE(VB_SIPARSER, LOC +
                QString("SDT: sid=%1 type=%2 eit_present=%3 "
                        "name=%5")
                .arg(s.ServiceID).arg(s.ServiceType)
                .arg(s.EITPresent).arg(s.ServiceName));
    }
}

void SIParser::HandleEIT(const DVBEventInformationTable *eit)
{
    if (eit_helper)
        eit_helper->AddEIT(eit);
}

/*------------------------------------------------------------------------
 * ATSC Table handlers
 *------------------------------------------------------------------------*/

void SIParser::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleMGT()");
    if (SI_STANDARD_AUTO == table_standard)
    {
        table_standard = SI_STANDARD_ATSC;
        VERBOSE(VB_SIPARSER, LOC + "Table Standard Detected: ATSC");
    }

    eit_reset = true;
    atsc_eit_pid.clear();
    atsc_ett_pid.clear();
    for (uint i = 0 ; i < mgt->TableCount(); i++)
    {
        const int table_class = mgt->TableClass(i);
        const uint pid        = mgt->TablePID(i);
        QString msg = QString(" on PID 0x%2").arg(pid,0,16);
        if (table_class == TableClass::TVCTc)
        {
            VERBOSE(VB_SIPARSER, LOC + "TVCT" + msg);
        }
        else if (table_class == TableClass::CVCTc)
        {
            VERBOSE(VB_SIPARSER, LOC + "CVCT" + msg);
        }
        else if (table_class == TableClass::ETTc)
        {
            VERBOSE(VB_SIPARSER, LOC + "Channel ETT" + msg);
        }
        else if (table_class == TableClass::EIT)
        {
            const uint num = mgt->TableType(i) - 0x100;
            atsc_eit_pid[num] = pid;
        }
        else if (table_class == TableClass::ETTe)
        {
            const uint num = mgt->TableType(i) - 0x200;
            atsc_ett_pid[num] = pid;
        }
        else
        {
            VERBOSE(VB_SIPARSER, LOC + "Unused Table " +
                    mgt->TableClassString(i) + msg);
        }                    
    }
}

void SIParser::HandleVCT(uint pid, const VirtualChannelTable *vct)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleVCT("<<pid<<") cnt("
            <<vct->ChannelCount()<<")");

    sourceid_to_channel.clear();
    for (uint chan_idx = 0; chan_idx < vct->ChannelCount() ; chan_idx++)
    {
        if (vct->IsHiddenInGuide(chan_idx))
        {
            VERBOSE(VB_EIT, LOC + QString("%1 chan %2-%3 is hidden in guide")
                    .arg(vct->ModulationMode(chan_idx) == 1 ? "NTSC" : "ATSC")
                    .arg(vct->MajorChannel(chan_idx))
                    .arg(vct->MinorChannel(chan_idx)));
            continue;
        }

        if (1 == vct->ModulationMode(chan_idx))
        {
            VERBOSE(VB_EIT, LOC + QString("Ignoring NTSC chan %1-%2")
                    .arg(vct->MajorChannel(chan_idx))
                    .arg(vct->MinorChannel(chan_idx)));
            continue;
        }

        VERBOSE(VB_EIT, LOC + QString("Adding Source #%1 ATSC chan %2-%3")
                .arg(vct->SourceID(chan_idx))
                .arg(vct->MajorChannel(chan_idx))
                .arg(vct->MinorChannel(chan_idx)));

        sourceid_to_channel[vct->SourceID(chan_idx)] =
            vct->MajorChannel(chan_idx) << 8 | vct->MinorChannel(chan_idx);
    }
}

void SIParser::HandleEIT(uint /*pid*/, const EventInformationTable *eit)
{
    const uint atscsrcid = sourceid_to_channel[eit->SourceID()];
    if (atscsrcid && eit_helper)
        eit_helper->AddEIT(atscsrcid, eit);
}

void SIParser::HandleETT(uint /*pid*/, const ExtendedTextTable *ett)
{
    if (ett->IsEventETM()) // decode guide ETTs
    {
        const uint atscsrcid = sourceid_to_channel[ett->SourceID()];
        if (atscsrcid && eit_helper)
            eit_helper->AddETT(atscsrcid, ett);
    }
}

void SIParser::HandleSTT(const SystemTimeTable *stt)
{
    if (eit_helper &&
        (atsc_stream_data->GPSOffset() != eit_helper->GetGPSOffset()))
    {
        VERBOSE(VB_SIPARSER, LOC + stt->toString());
        eit_helper->SetGPSOffset(atsc_stream_data->GPSOffset());
    }
}

/* vim: set sw=4 expandtab: */

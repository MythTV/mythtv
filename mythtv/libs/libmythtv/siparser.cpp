// Std C++ headers
#include <algorithm>

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
#include "dvbtypes.h"

// Set EIT_DEBUG_SID to a valid serviceid to enable EIT debugging
//#define EIT_DEBUG_SID 1602

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
    pmap_lock(false),
    // State variables
    ThreadRunning(false),           exitParserThread(false),
    eit_dn_long(false),
    PrivateTypesLoaded(false)
{
    SetATSCStreamData(new ATSCStreamData(-1,-1));
    SetDVBStreamData(new DVBStreamData());

    /* Set the PrivateTypes to default values */
    PrivateTypes.reset();

    /* Initialize the Table Handlers */
    Table[SERVICES] = new ServiceHandler();
}

SIParser::~SIParser()
{
    pmap_lock.lock();
    for (uint i = 0; i < NumHandlers; ++i)
    {
        if (Table[i])
            delete Table[i];
    }
    pmap_lock.unlock();
}

void SIParser::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    PrintDescriptorStatistics();
    QObject::deleteLater();
}

/* Resets all trackers, and closes all section filters */
void SIParser::Reset(void)
{
    VERBOSE(VB_SIPARSER, LOC + "About to do a reset");

    PrintDescriptorStatistics();

    VERBOSE(VB_SIPARSER, LOC + "Closing all PIDs");
    DelAllPids();

    PrivateTypesLoaded = false;
    PrivateTypes.reset();

    VERBOSE(VB_SIPARSER, LOC + "Resetting all Table Handlers");

    {
        QMutexLocker locker(&pmap_lock);
        for (int x = 0; x < NumHandlers ; x++)
            Table[x]->Reset();
    }

    atsc_stream_data->Reset(-1,-1);
    dvb_stream_data->Reset();

    VERBOSE(VB_SIPARSER, LOC + "SIParser Reset due to channel change");
}

void SIParser::CheckTrackers()
{

    uint16_t pid;
    uint8_t filter,mask;

    pmap_lock.lock();

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
    // for DVB EIT
    if (SI_STANDARD_DVB == table_standard &&
        !dvb_srv_collect_eit.empty())
    {
        AddPid(DVB_EIT_PID, 0x00, 0xFF, true, 1000 /*bufferFactor*/);
        if (eit_dn_long)
        {
            AddPid(DVB_DNLONG_EIT_PID, 0x00, 0xFF,
                   true, 1000 /*bufferFactor*/);
        }
    }
    // for ATSC STT and MGT.
    if (SI_STANDARD_ATSC == table_standard)
        AddPid(ATSC_PSIP_PID, 0x00, 0xFF, true, 10 /*bufferFactor*/);

    if (SI_STANDARD_ATSC == table_standard &&
        !sourceid_to_channel.empty())
    {
        atsc_eit_pid_map_t::const_iterator it = atsc_eit_pid.begin();
        for (; it != atsc_eit_pid.end(); ++it)
        {
            VERBOSE(VB_SIPARSER, LOC + QString("EIT-%1 on pid(0x%2)")
                    .arg(it.key(),2).arg(*it,0,16));
            AddPid(*it, 0xFF, TableID::EIT, true, 10 /*bufferFactor*/);
        }
        atsc_eit_pid.clear();

        atsc_ett_pid_map_t::const_iterator it2 = atsc_ett_pid.begin();
        for (; it2 != atsc_ett_pid.end(); ++it2)
        {
            VERBOSE(VB_SIPARSER, LOC + QString("ETT-%1 on pid(0x%2)")
                    .arg(it2.key(),2).arg(*it2,0,16));
            AddPid(*it2, 0xFF, TableID::ETT, true, 10 /*bufferFactor*/);
        }
        atsc_ett_pid.clear();
    }

    if (SI_STANDARD_DVB == table_standard)
    {
        if (dvb_stream_data->HasAllNITSections())
        {
            Table[SERVICES]->DependencyMet(NETWORK);
        }
    }

    pmap_lock.unlock();
}

void SIParser::LoadPrivateTypes(uint networkID)
{
    // If we've already loaded this stuff, do nothing
    if (PrivateTypesLoaded)
        return;

    QString stdStr = SIStandard_to_String(table_standard);

    // If you don't know the Table Standard yet you need to bail out
    if (stdStr == "auto")
        return;

    PrivateTypesLoaded = true;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT private_type, private_value "
        "FROM dtv_privatetypes "
        "WHERE networkid = :NETID AND "
        "      sitype    = :SITYPE");
    query.bindValue(":NETID",  networkID);
    query.bindValue(":SITYPE", stdStr);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Loading Private Types for SIParser", query);
        return;
    }

    if (!query.size())
    {
        VERBOSE(VB_SIPARSER, LOC + "No Private Types defined " +
                QString("for NetworkID %1").arg(networkID));
        return;
    }

    while (query.next())
    {
        QString key = query.value(0).toString();
        QString val = query.value(1).toString();

        VERBOSE(VB_SIPARSER, LOC +
                QString("Private Type %1 = %2 defined for NetworkID %3")
                .arg(key).arg(val).arg(networkID));

        if (key == "sdt_mapping")
        {
            VERBOSE(VB_SIPARSER, LOC +
                    "SDT Mapping Incorrect for this Service Fixup Loaded");

            PrivateTypes.SDTMapping = true;
        }
        else if (key == "channel_numbers")
        {
            int cn = val.toInt();
            VERBOSE(VB_SIPARSER, LOC + "ChannelNumbers Present using " +
                    QString("Descriptor %1").arg(cn));

            PrivateTypes.ChannelNumbers = cn;
        }
        else if (key == "force_guide_present" && val == "yes")
        {
            VERBOSE(VB_SIPARSER, LOC + "Forcing Guide Present");
            PrivateTypes.ForceGuidePresent = true;
        }
        if (key == "guide_fixup")
        {
            int fxup = val.toInt();
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Using Guide Fixup Scheme #%1").arg(fxup));

            PrivateTypes.EITFixUp = fxup;
        }
        if (key == "guide_ranges")
        {
            const QStringList temp = QStringList::split(",", val);
            PrivateTypes.CustomGuideRanges = true;
            PrivateTypes.CurrentTransportTableMin = temp[0].toInt();
            PrivateTypes.CurrentTransportTableMax = temp[1].toInt();
            PrivateTypes.OtherTransportTableMin   = temp[2].toInt();
            PrivateTypes.OtherTransportTableMax   = temp[3].toInt();
                
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Using Guide Custom Range; "
                            "CurrentTransport: %1-%2, "
                            "OtherTransport: %3-%4")
                    .arg(PrivateTypes.CurrentTransportTableMin,2,16)
                    .arg(PrivateTypes.CurrentTransportTableMax,2,16)
                    .arg(PrivateTypes.OtherTransportTableMin,2,16)
                    .arg(PrivateTypes.OtherTransportTableMax,2,16));
        }
        if (key == "tv_types")
        {
            PrivateTypes.TVServiceTypes.clear();
            const QStringList temp = QStringList::split(",", val);
            QStringList::const_iterator it = temp.begin();
            for (; it != temp.end() ; it++)
            {
                int stype = (*it).toInt();
                PrivateTypes.TVServiceTypes[stype] = 1;
                VERBOSE(VB_SIPARSER, LOC +
                        QString("Added TV Type %1").arg(stype));
            }
        }
#ifdef USING_DVB_EIT
#if 0
        if (key == "parse_subtitle_list")
        {
            eitfixup.clearSubtitleServiceIDs();
            const QStringList temp = QStringList::split(",", val);
            QStringList::const_iterator it = temp.begin();
            for (; it!=temp.end(); it++)
            {
                uint sid = (*it).toUInt();
                VERBOSE(VB_SIPARSER, LOC + 
                        QString("Added ServiceID %1 to list of "
                                "channels to parse subtitle from").arg(sid));

                eitfixup.addSubtitleServiceID(sid);
            }
        }
#endif
#endif //USING_DVB_EIT
    }
}

void SIParser::SetATSCStreamData(ATSCStreamData *stream_data)
{
    VERBOSE(VB_IMPORTANT, LOC + "Setting ATSCStreamData");

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

#ifdef USING_DVB_EIT
    connect(atsc_stream_data,
            SIGNAL(UpdateEIT(uint, const EventInformationTable*)),
            this,
            SLOT(  HandleEIT(uint, const EventInformationTable*)));
    connect(atsc_stream_data,
            SIGNAL(UpdateETT(uint, const ExtendedTextTable*)),
            this,
            SLOT(  HandleETT(uint, const ExtendedTextTable*)));
#endif // USING_DVB_EIT
}

void SIParser::SetDVBStreamData(DVBStreamData *stream_data)
{
    VERBOSE(VB_IMPORTANT, LOC + "Setting DVBStreamData");

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

#ifdef USING_DVB_EIT
    connect(dvb_stream_data,
            SIGNAL(UpdateEIT(const DVBEventInformationTable*)),
            this,
            SLOT(  HandleEIT(const DVBEventInformationTable*)));
#endif // USING_DVB_EIT
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

        LoadPrivateTypes(sdt.OriginalNetworkID());

        bool cur =
            (PrivateTypes.SDTMapping &&
             PrivateTypes.CurrentTransportID == sdt.TSID()) ||
            (!PrivateTypes.SDTMapping && TableID::SDT == sdt.TableID());

        uint sect_tsid = (cur) ? 0 : sdt.TSID();

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

void SIParser::HandlePAT(const ProgramAssociationTable *pat)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("PAT Version: %1  Tuned to TransportID: %2")
            .arg(pat->Version()).arg(pat->TransportStreamID()));

    PrivateTypes.CurrentTransportID = pat->TransportStreamID();
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
    const desc_list_t list = MPEGDescriptor::Parse(
        cat->Descriptors(), cat->DescriptorsLength());

    for (uint i = 0; i < list.size(); i++)
    {
        if (DescriptorID::conditional_access != list[i][0])
        {
            CountUnusedDescriptors(0x1, list[i]);
            continue;
        }

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
 *   DVB TABLE PARSERS
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

    // count the unused descriptors for debugging.
    for (uint i = 0; i < dlist.size(); i++)
    {
        if (DescriptorID::network_name == dlist[i][0])
            continue;
        if (DescriptorID::linkage == dlist[i][0])
            continue;
        CountUnusedDescriptors(0x10, dlist[i]);
    }
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
        else if (DescriptorID::dvb_uk_channel_list == dlist[i][0] &&
                 DescriptorID::dvb_uk_channel_list ==
                 PrivateTypes.ChannelNumbers)
        {
            UKChannelListDescriptor ucld(dlist[i]);
            for (uint i = 0; i < ucld.ChannelCount(); i++)
                clist[ucld.ServiceID(i)] = ucld.ChannelNumber(i);
        }
        else
        {
            CountUnusedDescriptors(0x10, dlist[i]);
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
        LoadPrivateTypes(nit->OriginalNetworkID(i));

        const desc_list_t dlist = MPEGDescriptor::Parse(
            nit->TransportDescriptors(i), nit->TransportDescriptorsLength(i));

        QMap_uint16_t chanNums;
        HandleNITTransportDesc(dlist, t, chanNums);

        t.TransportID = nit->TSID(i);
        t.NetworkID   = nit->OriginalNetworkID(i);

        sh->Request(nit->TSID(i));
    
        if (PrivateTypes.ChannelNumbers && !(chanNums.empty()))
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

    bool cur =
        (PrivateTypes.SDTMapping &&
         PrivateTypes.CurrentTransportID == sdt->TSID()) ||
        (!PrivateTypes.SDTMapping && TableID::SDT == sdt->TableID());

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        SDTObject s;
        s.Version       = sdt->Version();
        s.TransportID   = sdt->TSID();
        s.NetworkID     = sdt->OriginalNetworkID();

        s.ServiceID     = sdt->ServiceID(i);
        bool has_eit    = PrivateTypes.ForceGuidePresent;
        has_eit        |= sdt->HasEITSchedule(i);
        has_eit        |= sdt->HasEITPresentFollowing(i);
        s.EITPresent    = (has_eit) ? 1 : 0;
        s.RunningStatus = sdt->RunningStatus(i);
        s.CAStatus      = sdt->IsEncrypted(i) ? 1 : 0;

        // Rename channel with info from DB
        if (slist.contains(sdt->ServiceID(i)))
            s.ChanNum = slist[sdt->ServiceID(i)].ChanNum;

        const desc_list_t list = MPEGDescriptor::Parse(
            sdt->ServiceDescriptors(i), sdt->ServiceDescriptorsLength(i));
        for (uint j = 0; j < list.size(); j++)
        {
            if (DescriptorID::service == list[j][0])
            {
                ServiceDescriptor sd(list[j]);
                s.ServiceType  = sd.ServiceType();
                s.ProviderName = sd.ServiceProviderName();
                s.ServiceName  = sd.ServiceName();

                if (PrivateTypes.TVServiceTypes.contains(s.ServiceType))
                    s.ServiceType = PrivateTypes.TVServiceTypes[s.ServiceType];
                continue;
            }
            CountUnusedDescriptors(0x11, list[j]);
        }

        // Check if we should collect EIT on this transport
        bool is_tv_or_radio = (s.ServiceType == SDTObject::TV ||
                               s.ServiceType == SDTObject::RADIO);

        bool is_eit_transport = !PrivateTypes.GuideOnSingleTransport;
        is_eit_transport |= PrivateTypes.GuideOnSingleTransport && 
            (PrivateTypes.GuideTransportID == PrivateTypes.CurrentTransportID);

        bool collect_eit = (has_eit && is_tv_or_radio && is_eit_transport);
        if (collect_eit)
            dvb_srv_collect_eit[sdt->ServiceID(i)] = true;

        uint sect_tsid = (cur) ? 0 : sdt->TSID();
        sh->Services[sect_tsid][sdt->ServiceID(i)] = s;

        VERBOSE(VB_SIPARSER, LOC +
                QString("SDT: sid=%1 type=%2 eit_present=%3 "
                        "collect_eit=%4 name=%5")
                .arg(s.ServiceID).arg(s.ServiceType)
                .arg(s.EITPresent).arg(collect_eit)
                .arg(s.ServiceName));
    }
}

void SIParser::HandleEIT(const DVBEventInformationTable *eit)
{
    if (eithelper)
        eithelper->AddEIT(eit);
}

/*------------------------------------------------------------------------
 *   ATSC TABLE PARSERS
 *------------------------------------------------------------------------*/

/*
 *  ATSC Table 0xC7 - Master Guide Table - PID 0x1FFB
 */
void SIParser::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleMGT()");
    if (SI_STANDARD_AUTO == table_standard)
    {
        table_standard = SI_STANDARD_ATSC;
        VERBOSE(VB_SIPARSER, LOC + "Table Standard Detected: ATSC");
    }

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
#ifdef USING_DVB_EIT
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
#endif // USING_DVB_EIT
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

    for (uint chan_idx = 0; chan_idx < vct->ChannelCount() ; chan_idx++)
    {
        // Do not add in Analog Channels in the VCT
        if (1 == vct->ModulationMode(chan_idx))
            continue;

        // Create SDTObject from info in VCT
        SDTObject s;
        s.Version      = vct->Version();
        s.ServiceType  = 1;
        s.EITPresent   = !vct->IsHiddenInGuide(chan_idx);

        s.ServiceName  = vct->ShortChannelName(chan_idx);
        s.ChanNum      =(vct->MajorChannel(chan_idx) * 10 +
                         vct->MinorChannel(chan_idx));
        s.TransportID  = vct->ChannelTransportStreamID(chan_idx);
        s.CAStatus     = vct->IsAccessControlled(chan_idx);
        s.ServiceID    = vct->ProgramNumber(chan_idx);
        s.ATSCSourceID = vct->SourceID(chan_idx);

        LoadPrivateTypes(s.TransportID);

#ifdef USING_DVB_EIT
        if (!vct->IsHiddenInGuide(chan_idx))
        {
            VERBOSE(VB_EIT, LOC + "Adding Source #"<<s.ATSCSourceID
                    <<" ATSC chan "<<vct->MajorChannel(chan_idx)
                    <<"-"<<vct->MinorChannel(chan_idx));
            sourceid_to_channel[s.ATSCSourceID] =
                vct->MajorChannel(chan_idx) << 8 | vct->MinorChannel(chan_idx);
        }
        else
        {
            VERBOSE(VB_EIT, LOC + "ATSC chan "<<vct->MajorChannel(chan_idx)
                    <<"-"<<vct->MinorChannel(chan_idx)<<" is hidden in guide");
        }
#endif

        ((ServiceHandler*) Table[SERVICES])->Services[0][s.ServiceID] = s;
    }
}

/*
 *  ATSC Table 0xCB - Event Information Table - PID Varies
 */
void SIParser::HandleEIT(uint /*pid*/, const EventInformationTable *eit)
{
    const uint atscsrcid = sourceid_to_channel[eit->SourceID()];
    if (atscsrcid && eithelper)
        eithelper->AddEIT(atscsrcid, eit);
}

/*
 *  ATSC Table 0xCC - Extended Text Table - PID Varies
 */
void SIParser::HandleETT(uint /*pid*/, const ExtendedTextTable *ett)
{
    if (ett->IsEventETM()) // decode guide ETTs
    {
        const uint atscsrcid = sourceid_to_channel[ett->SourceID()];
        if (atscsrcid && eithelper)
            eithelper->AddETT(atscsrcid, ett);
    }
}

/*
 *  ATSC Table 0xCD - System Time Table - PID 0x1FFB
 */
void SIParser::HandleSTT(const SystemTimeTable *stt)
{
    if (eithelper &&
        (atsc_stream_data->GPSOffset() != eithelper->GetGPSOffset()))
    {
        VERBOSE(VB_SIPARSER, LOC + stt->toString());
        eithelper->SetGPSOffset(atsc_stream_data->GPSOffset());
    }
}

void SIParser::CountUnusedDescriptors(uint pid, const unsigned char *data)
{
    if (!(print_verbose_messages & VB_SIPARSER))
        return;

    QMutexLocker locker(&descLock);
    descCount[pid<<8 | data[0]]++;
}

void SIParser::PrintDescriptorStatistics(void) const
{
    if (!(print_verbose_messages & VB_SIPARSER))
        return;

    QMutexLocker locker(&descLock);
    VERBOSE(VB_SIPARSER, LOC + "Descriptor Stats -- begin");
    QMap<uint,uint>::const_iterator it = descCount.begin();
    for (;it != descCount.end(); ++it)
    {
        uint pid = it.key() >> 8;
        uint cnt = (*it);
        unsigned char sid = it.key() & 0xff;
        VERBOSE(VB_SIPARSER, LOC +
                QString("On PID 0x%1: Found %2, %3 Descriptor%4")
                .arg(pid,0,16).arg(cnt)
                .arg(MPEGDescriptor(&sid).DescriptorTagString())
                .arg((cnt>1) ? "s" : ""));
    }
    VERBOSE(VB_SIPARSER, LOC + "Descriptor Stats -- end");
}

/* vim: set sw=4 expandtab: */

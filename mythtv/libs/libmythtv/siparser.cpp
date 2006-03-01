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
#include "dvbtables.h"
#include "dishdescriptors.h"

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
    SIStandard(SI_STANDARD_AUTO),
    CurrentTransport(0),            RequestedServiceID(0),
    RequestedTransportID(0),        NITPID(0),
    // ATSC Storage
    atsc_stream_data(new ATSCStreamData(-1,-1)),
    // Mutex Locks
    pmap_lock(false),
    // State variables
    ThreadRunning(false),           exitParserThread(false),
    ParserInReset(false),           standardChange(false),
    eit_dn_long(false),
    PrivateTypesLoaded(false)
{
    /* Set the PrivateTypes to default values */
    PrivateTypes.reset();

    /* Initialize the Table Handlers */
    Table[PAT]      = new PATHandler();
    Table[PMT]      = new PMTHandler();
    Table[SERVICES] = new ServiceHandler();
    Table[NETWORK]  = new NetworkHandler();
#ifdef USING_DVB_EIT
    Table[EVENTS]   = new EventHandler();
#endif //USING_DVB_EIT

    Reset();

    // Get a list of wanted languages and set up their priorities
    // (Lowest number wins)
    QStringList langPref = iso639_get_language_list();
    QStringList::Iterator plit;
    uint prio = 1;
    for (plit = langPref.begin(); plit != langPref.end(); ++plit)
    {
        if (!(*plit).isEmpty())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Added initial preferred language '%1' "
                            "with priority %2").arg(*plit).arg(prio));
            LanguagePriority[*plit] = prio++;
        }
    }

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
    connect(atsc_stream_data, SIGNAL(UpdateETT(uint,const ExtendedTextTable*)),
            this,             SLOT(HandleETT(uint,const ExtendedTextTable*)));
#endif // USING_DVB_EIT
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
void SIParser::Reset()
{
    ParserInReset = true;

    // Close all pids that are open

    VERBOSE(VB_SIPARSER, LOC + "About to do a reset");

    PrintDescriptorStatistics();

    VERBOSE(VB_SIPARSER, LOC + "Closing all PIDs");
    DelAllPids();

    PrivateTypesLoaded = false;
    PrivateTypes.reset();

    VERBOSE(VB_SIPARSER, LOC + "Resetting all Table Handlers");
    pmap_lock.lock();

    for (int x = 0; x < NumHandlers ; x++)
        Table[x]->Reset();

    pmap_lock.unlock();

    atsc_stream_data->Reset(-1,-1);

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
                int bufferFactor = 10;
#ifdef USING_DVB_EIT
                if ((SIStandard == SI_STANDARD_DVB) && (x == EVENTS))
                    bufferFactor = 1000;
#endif // USING_DVB_EIT
                AddPid(pid, mask, filter, true, bufferFactor);
            }
        }
    }
    // for ATSC STT and MGT.
    if (SIStandard == SI_STANDARD_ATSC)
        AddPid(ATSC_PSIP_PID, 0x00, 0xFF, true, 10 /*bufferFactor*/);

    uint16_t key0 = 0,key1 = 0;

    /* See if any new objects require an emit */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->EmitRequired())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Table[%1]->EmitRequired() == true")
                    .arg((tabletypes) x));
            switch (x)
            {
                case PMT:
                    while (Table[PMT]->GetEmitID(key0, key1))
                    {
                        PMTHandler *hdl = (PMTHandler*) Table[PMT];
                        const PMTObject &pmt = hdl->pmt[key0];
                        emit UpdatePMT(&pmt);
                    }
                    break;
#ifdef USING_DVB_EIT
                case EVENTS:
                    while (Table[EVENTS]->GetEmitID(key0,key1))
                    {
                        EventHandler *hdl = (EventHandler*) Table[EVENTS];
                        emit EventsReady(&(hdl->Events[key0]));
                    }
                    break;
#endif // USING_DVB_EIT
                case NETWORK:
                    while(Table[NETWORK]->GetEmitID(key0,key1))
                        emit FindTransportsComplete();
                    break;
                default:
                    break;
            }
        }
    }

    pmap_lock.unlock();


}

void SIParser::LoadPrivateTypes(uint networkID)
{
    // If we've already loaded this stuff, do nothing
    if (PrivateTypesLoaded)
        return;

    QString stdStr = SIStandard_to_String(SIStandard);

    // If you don't know the SI Standard yet you need to bail out
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
#endif //USING_DVB_EIT
    }
}

bool SIParser::GetTransportObject(NITObject &NIT)
{
    pmap_lock.lock();
    NIT = ((NetworkHandler*) Table[NETWORK])->NITList;
    pmap_lock.unlock();
    return true;
}

bool SIParser::GetServiceObject(QMap_SDTObject &SDT)
{

    pmap_lock.lock();
    SDT = ((ServiceHandler*) Table[SERVICES])->Services[0];
    pmap_lock.unlock();
    return true;
}

void SIParser::AddPid(uint16_t pid, uint8_t mask, uint8_t filter,
                      bool CheckCRC, int bufferFactor)
{
    (void) pid;
    (void) mask;
    (void) filter;
    (void) CheckCRC;
    (void) bufferFactor;
    VERBOSE(VB_SIPARSER, LOC + "AddPid() does nothing");
}

void SIParser::DelPid(int pid)
{
    (void) pid;
    VERBOSE(VB_SIPARSER, LOC + "DelPid() does nothing");
}

void SIParser::DelAllPids()
{
    VERBOSE(VB_SIPARSER, LOC + "DelAllPids does nothing");
}

/** \fn SIParser::FillPMap(SISTANDARD _SIStandard)
 *  \brief Adds basic PID's corresponding to standard to the list
 *         of PIDs we are listening to.
 *
 *   For ATSC this adds the PAT (0) and MGT (0x1ffb) pids.
 *
 *   For DVB this adds the PAT(0), SDT (0x11), and NIT (0x10) pids.
 *
 *   Note: This actually adds all of the above so as to simplify channel
 *         scanning, but this may change as this can break ATSC.
 */
bool SIParser::FillPMap(SISTANDARD _SIStandard)
{
    VERBOSE(VB_SIPARSER, QString("FillPMap(SIS %1)")
            .arg((SI_STANDARD_ATSC == _SIStandard) ? "atsc":"dvb"));

    pmap_lock.lock();
    VERBOSE(VB_SIPARSER, LOC + "Requesting PAT");

    /* By default open only the PID for PAT */
    Table[PAT]->Request(0);
    Table[SERVICES]->Request(0);
    Table[NETWORK]->Request(0);

    for (int x = 0 ; x < NumHandlers ; x++)
        Table[x]->SetSIStandard(_SIStandard);

    SIStandard = _SIStandard;

    pmap_lock.unlock();

    return true;
}

/** \fn SIParser::FillPMap(const QString&)
 *  \brief Adds basic PID's corresponding to standard to the list
 *         of PIDs we are listening to.
 *
 *   This is a convenience function that calls SIParser::FillPMap(SISTANDARD)
 */
bool SIParser::FillPMap(const QString &si_std)
{
    VERBOSE(VB_SIPARSER, QString("FillPMap(str %1)").arg(si_std));
    bool is_atsc = si_std.lower() == "atsc";
    return FillPMap((is_atsc) ? SI_STANDARD_ATSC : SI_STANDARD_DVB);
}

/** \fn SIParser::AddPMT(uint16_t ServiceID)
 *  \brief Adds the pid associated with program number 'ServiceID' to the
 *         listening list if this is an ATSC stream and adds the pid associated
 *         with service with service id 'ServiceID' to the listending list if
 *         this is a DVB stream.
 *
 *   If the table standard is ATSC then this adds the pid of the PMT listed
 *   as program number 'ServiceID' in the PAT to the list of pids we want.
 *
 *   If the table standard is DVB then this looks up the program number
 *   of the service listed with the 'ServiceID' in the SDT, then it looks
 *   up that program number in the PAT table and adds the associated pid
 *   to the list of pids we want.
 *
 *   Further the pid is identified as carrying a PMT, so a UpdatePMT Qt
 *   signal is emitted when a table is seen on this pid. Normally this
 *   signal is connected to the DVBChannel class, which relays the update
 *   to the DVBCam class which handles decrypting encrypted services.
 *
 *  \param ServiceID Either the the MPEG Program Number or the DVB Service ID
 *  \return true on success, false on failure.
 */
bool SIParser::AddPMT(uint16_t ServiceID)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Adding PMT program number #%1 to the request list")
            .arg(ServiceID));

    pmap_lock.lock();
    Table[PMT]->RequestEmit(ServiceID);
    pmap_lock.unlock();

    atsc_stream_data->Reset(-1, -1);
    ((MPEGStreamData*)atsc_stream_data)->Reset(ServiceID);
    atsc_stream_data->AddListeningPID(ATSC_PSIP_PID);

    return true;
}

/** \fn SIParser::ReinitSIParser(const QString&, uint)
 *  \brief Convenience function that calls FillPMap(SISTANDARD) and
 *         AddPMT(uint)
 */
bool SIParser::ReinitSIParser(const QString &si_std, uint service_id)
{
    VERBOSE(VB_SIPARSER, LOC + QString("ReinitSIParser(std %1, %2 #%3)")
             .arg(si_std)
             .arg((si_std.lower() == "atsc") ? "program" : "service")
             .arg(service_id));

    return FillPMap(si_std) & AddPMT(service_id);
}


bool SIParser::FindTransports()
{
    Table[NETWORK]->RequestEmit(0);
    return true;
}

bool SIParser::FindServices()
{
    Table[SERVICES]->RequestEmit(0);
    return true;
}

/*------------------------------------------------------------------------
 *   COMMON PARSER CODE
 *------------------------------------------------------------------------*/

void SIParser::ParseTable(uint8_t *buffer, int size, uint16_t pid)
{
    QMutexLocker locker(&pmap_lock);

    tablehead_t head = ParseTableHead(buffer, size);
    const PESPacket pes = PESPacket::ViewData(buffer);
    const PSIPTable psip(pes);

    if (!psip.SectionSyntaxIndicator())
    {
        VERBOSE(VB_SIPARSER, LOC + "section_syntax_indicator == 0: " +
                QString("Ignoring table 0x%1").arg(psip.TableID(),0,16));
        return;
    }

    // In Detection mode determine what SIStandard you are using.
    if (SIStandard == SI_STANDARD_AUTO)
    {
        if (TableID::MGT == psip.TableID())
        {
            SIStandard = SI_STANDARD_ATSC;
            VERBOSE(VB_SIPARSER, LOC + "SI Standard Detected: ATSC");
            standardChange = true;
        }
        else if (TableID::SDT  == psip.TableID() ||
                 TableID::SDTo == psip.TableID())
        {
            SIStandard = SI_STANDARD_DVB;
            VERBOSE(VB_SIPARSER, LOC + "SI Standard Detected: DVB");
            standardChange = true;
        }
    }

    // Parse tables for ATSC
    if (SIStandard == SI_STANDARD_ATSC)
    {
        atsc_stream_data->HandleTables(pid, psip);
        return;
    }

    // Parse MPEG tables
    if (SIStandard == SI_STANDARD_DVB)
    {
        if (TableID::PAT == psip.TableID() &&
            !((PATHandler*) Table[PAT])->Tracker.AddSection(&head))
        {
            ProgramAssociationTable pat(psip);
            HandlePAT(&pat);
        }
        else if (TableID::CAT == psip.TableID())
        {
            ConditionalAccessTable cat(psip);
            HandleCAT(&cat);
        }
        else if (TableID::PMT == psip.TableID() &&
                 !Table[PMT]->AddSection(&head, psip.TableIDExtension(), 0))
        {
            ProgramMapTable pmt(psip);
            HandlePMT(pmt.ProgramNumber(), &pmt);
        }
    }

    // Parse DVB specific NIT and SDT tables
    if (SIStandard == SI_STANDARD_DVB)
    {
        if (TableID::NIT == psip.TableID() &&
            !Table[NETWORK]->AddSection(&head,0,0))
        {
            // Network Information Table
            NetworkInformationTable nit(psip);
            HandleNIT(&nit);
        }
        else if (TableID::SDT  == psip.TableID() ||
                 TableID::SDTo == psip.TableID())
        {
            // Service Tables
            ServiceDescriptionTable sdt(psip);

            emit TableLoaded(); // Signal to keep scan wizard bars moving

            LoadPrivateTypes(sdt.OriginalNetworkID());

            bool cur =
                (PrivateTypes.SDTMapping &&
                 PrivateTypes.CurrentTransportID == sdt.TSID()) ||
                (!PrivateTypes.SDTMapping && TableID::SDT == sdt.TableID());

            uint sect_tsid = (cur) ? 0 : sdt.TSID();

            if (!Table[SERVICES]->AddSection(&head, sect_tsid, 0))
                HandleSDT(sect_tsid, &sdt);
        }
    }

#ifdef USING_DVB_EIT
    // Parse any embedded guide data
    if (SIStandard == SI_STANDARD_DVB &&
        DVBEventInformationTable::IsEIT(psip.TableID()))
    {
        EventHandler *eh         = (EventHandler*) Table[EVENTS];
        uint tableid             = psip.TableID();
        uint serviceid           = psip.TableIDExtension();
        uint last_segment_number = buffer[8+4];
        uint last_table_id       = buffer[8+5];

        if (!eh->TrackerSetup[serviceid])
        {
            if (PrivateTypes.CustomGuideRanges)
            {
                // current
                uint cstart = PrivateTypes.CurrentTransportTableMin;
                uint cend   = PrivateTypes.CurrentTransportTableMax;
                if ((tableid >= cstart) && (tableid <= cend))
                {
                    for (uint i = cstart; i <= cend; i++)
                        eh->Tracker[serviceid][i].Reset();
                }

                // other
                uint ostart = PrivateTypes.OtherTransportTableMin;
                uint oend   = PrivateTypes.OtherTransportTableMax;
                if ((tableid >= ostart) && (tableid <= oend))
                {
                    for (uint i = ostart; i <= oend; i++)
                        eh->Tracker[serviceid][i].Reset();
                }
            }
            else
            {
                // current
                if ((tableid & 0xF0) == 0x50)
                {
                    for (uint i = 0; i < (last_table_id & 0x0F); i++)
                        eh->Tracker[serviceid][0x50|i].Reset();
                }
                // other
                if ((tableid & 0xF0) == 0x60)
                {
                    for (uint i = 0; i < (last_table_id & 0x0F); i++)
                        eh->Tracker[serviceid][0x60|i].Reset();
                }
            }
            eh->TrackerSetup[serviceid] = true;
        }
        
        if (!Table[EVENTS]->AddSection(&head, serviceid, tableid))
        {
            if (last_segment_number != head.section_last)
            {
                uint start = last_segment_number + 1;
                uint end   = head.section_number | 7;
                for (uint i = start; i <= end; i++)
                    eh->Tracker[serviceid][tableid].MarkUnused(i);
            }

            const DVBEventInformationTable eit(psip);
            HandleEIT(&eit);
        }
    }
#endif // USING_DVB_EIT
}

tablehead_t SIParser::ParseTableHead(uint8_t *buffer, int size)
{
// TODO: Maybe handle the size but should be OK if CRC passes

    (void) size;
    tablehead_t head;

    head.table_id       = buffer[0];
    head.section_length = ((buffer[1] & 0x0f)<<8) | buffer[2];
    head.table_id_ext   = (buffer[3] << 8) | buffer[4];
    head.current_next   = (buffer[5] & 0x1);
    head.version        = ((buffer[5] >> 1) & 0x1f);
    head.section_number = buffer[6];
    head.section_last   = buffer[7];

    return head;

}

void SIParser::HandlePAT(const ProgramAssociationTable *pat)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("PAT Version: %1  Tuned to TransportID: %2")
            .arg(pat->Version()).arg(pat->TransportStreamID()));

    PATHandler *path = (PATHandler*) Table[PAT];
    PMTHandler *pmth = (PMTHandler*) Table[PMT];

    PrivateTypes.CurrentTransportID = pat->TransportStreamID();
    for (uint i = 0; i < pat->ProgramCount(); ++i)
    {
        // DVB Program 0 in the PAT represents the location of the NIT
        if (0 == pat->ProgramNumber(i) && SI_STANDARD_ATSC != SIStandard)
        {
            VERBOSE(VB_SIPARSER, LOC + "NIT Present on this transport " +
                    QString(" on PID 0x%1").arg(NITPID,0,16));

            NITPID = pat->ProgramPID(i);
            continue;
        }

        path->pids[pat->ProgramNumber(i)] = pat->ProgramPID(i);

        pmth->AddKey(pat->ProgramPID(i), 0);
        pmth->pmt[pat->ProgramNumber(i)].PMTPID = pat->ProgramPID(i);
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
    VERBOSE(VB_SIPARSER, LOC + QString("PMT pn(%1) version(%2) cnt(%3)")
            .arg(pnum).arg(pmt->Version()).arg(pmt->StreamCount()));

    // Create a PMTObject and populate it
    PMTObject p;
    p.PCRPID    = pmt->PCRPID();
    p.ServiceID = pmt->ProgramNumber();

    // Process Program Info Descriptors
    const unsigned char *pi = pmt->ProgramInfo();
    desc_list_t list = MPEGDescriptor::Parse(pi, pmt->ProgramInfoLength());
    desc_list_t::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        if (DescriptorID::conditional_access == (*it)[0])
        {
            ConditionalAccessDescriptor ca(*it);

            CAPMTObject cad;
            cad.CASystemID  = ca.SystemID();
            cad.PID         = ca.PID();
            cad.Data_Length = ca.DataSize();
            if (ca.DataSize())
                memcpy(cad.Data, ca.Data(), ca.DataSize());

            p.CA.append(cad);
            p.hasCA = true;
        }
        else
        {
            unsigned char *tmp = new unsigned char[(*it)[1] + 2];
            memcpy(tmp, *it, (*it)[1] + 2);
            p.Descriptors.push_back(tmp);
        }
    }

    // Process streams
    for (uint i = 0; i < pmt->StreamCount(); i++)
    {
        VERBOSE(VB_SIPARSER, LOC +
                QString("PID: 0x%1").arg(pmt->StreamPID(i),0,16));

        const unsigned char *si = pmt->StreamInfo(i);
        desc_list_t list = MPEGDescriptor::Parse(si, pmt->StreamInfoLength(i));
        uint type = StreamID::Normalize(pmt->StreamType(i), list);

        // Reset Elementary PID object
        ElementaryPIDObject e;
        e.Reset();
        e.PID         = pmt->StreamPID(i);
        e.Orig_Type   = pmt->StreamType(i);

        desc_list_t::const_iterator it = list.begin();
        for (; it != list.end(); ++it)
        {
            if (DescriptorID::conditional_access == (*it)[0])
            {
                ConditionalAccessDescriptor ca(*it);

                CAPMTObject cad;
                cad.CASystemID  = ca.SystemID();
                cad.PID         = ca.PID();
                cad.Data_Length = ca.DataSize();
                if (ca.DataSize())
                    memcpy(cad.Data, ca.Data(), ca.DataSize());

                p.CA.append(cad);
                p.hasCA = true;
            }
            else if (DescriptorID::teletext == (*it)[0])
            {
                TeletextDescriptor ttd(*it);
                // TODO remove printout, here to encourage error reporting
                for (uint i = 0; i < ttd.StreamCount(); i++)
                {
                    VERBOSE(VB_GENERAL, LOC +
                            QString("Teletext %1: lang(%2) "
                                    "type(%3) mag(%4) page(%5)")
                            .arg(i).arg(ttd.LanguageString(i))
                            .arg(ttd.TeletextType(i))
                            .arg(ttd.TeletextMagazineNum(i))
                            .arg(ttd.TeletextPageNum(i)));
                }
            }
            else if (DescriptorID::subtitling == (*it)[0])
            {
                SubtitlingDescriptor sd(*it);
                // TODO remove printout, here to encourage error reporting
                for (uint i = 0; i < sd.StreamCount(); i++)
                {
                    VERBOSE(VB_GENERAL, LOC +
                            QString("Subtitle %1: lang(%2) "
                                    "type(%3) comp(%4) anc(%5)")
                            .arg(i).arg(sd.LanguageString(i))
                            .arg(sd.SubtitleType(i))
                            .arg(sd.CompositionPageID(i))
                            .arg(sd.AncillaryPageID(i)));
                }
            }
        }

        for (it = list.begin(); it != list.end(); ++it)
        {
            // Note: the saved streams have already been 
            // descrambled by the CAM so any CA descriptors 
            // should *not* be added to the descriptor list.
            if (DescriptorID::conditional_access != (*it)[0])
            {
                unsigned char *tmp = new unsigned char[(*it)[1] + 2];
                memcpy(tmp, *it, (*it)[1] + 2);
                e.Descriptors.push_back(tmp);
            }
        }

        p.Components += e; 
        p.hasVideo |= StreamID::IsVideo(type);
        p.hasAudio |= StreamID::IsAudio(type);
    }

    ((PMTHandler*) Table[PMT])->pmt[pmt->ProgramNumber()] = p;
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
       ((NetworkHandler*) Table[NETWORK])->NITList.Network += n;

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
    NetworkHandler *nh = (NetworkHandler*) Table[NETWORK];

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

        nh->NITList.Transport += t;
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
        s.CAStatus      = sdt->HasFreeCA(i) ? 1 : 0;

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

#ifdef USING_DVB_EIT
        if (collect_eit)
            Table[EVENTS]->RequestEmit(sdt->ServiceID(i));
#endif

        uint sect_tsid = (cur) ? 0 : sdt->TSID();
        sh->Services[sect_tsid][sdt->ServiceID(i)] = s;

        VERBOSE(VB_SIPARSER, LOC +
                QString("SDT: sid=%1 type=%2 eit_present=%3 "
                        "collect_eit=%4 name=%5")
                .arg(s.ServiceID).arg(s.ServiceType)
                .arg(s.EITPresent).arg(collect_eit)
                .arg(s.ServiceName));
    }

    if (cur)
        emit FindServicesComplete();

#ifdef USING_DVB_EIT
    // TODO: This is temp
    Table[EVENTS]->DependencyMet(SERVICES);
    //Table[EVENTS]->AddPid(0x12,0x00,0x00,true); // see ticket #755
    Table[EVENTS]->AddPid(0x12,0x7F,0x80,0x12); // see ticket #755
    if (eit_dn_long)
        Table[EVENTS]->AddPid(0x300,0x00,0x00,true);
#endif // USING_DVB_EIT
}

/** \fn SIParser::GetLanguagePriority(const QString&)
 *  \brief Returns the desirability of a particular language to the user.
 *
 *   The lower the returned number the more preferred the language is.
 *
 *   If the language does not exist in our list of languages it is
 *   inserted as a less desirable language than any of the 
 *   previously inserted languages.
 *
 *  \param language Three character ISO-639 language code.
 */
uint SIParser::GetLanguagePriority(const QString &language)
{
    QMap<QString,uint>::const_iterator it;
    it = LanguagePriority.find(language);

    // if this is an unknown language, check if the canonical version exists
    if (it == LanguagePriority.end())
    {
        QString clang = iso639_str_to_canonoical_str(language);
        it = LanguagePriority.find(clang);
        if (it != LanguagePriority.end())
        {
            VERBOSE(VB_SIPARSER, LOC +
                    QString("Added preferred language '%1' with same "
                            "priority as '%2'").arg(language).arg(clang));
            LanguagePriority[language] = *it;
        }
    }

    // if this is an unknown language, insert into priority list
    if (it == LanguagePriority.end())
    {
        // find maximum existing priority
        uint max_priority = 0;
        it = LanguagePriority.begin();
        for (;it != LanguagePriority.end(); ++it)
            max_priority = max(*it, max_priority);
        // insert new language, and point iterator to it
        VERBOSE(VB_SIPARSER, LOC +
                QString("Added preferred language '%1' with priority %2")
                .arg(language).arg(max_priority + 1));
        LanguagePriority[language] = max_priority + 1;
        it = LanguagePriority.find(language);
    }

    // return the priority
    return *it;
}

void SIParser::HandleEIT(const DVBEventInformationTable *eit)
{
#ifdef USING_DVB_EIT
    uint bestPrioritySE   = UINT_MAX;
    uint bestPriorityEE   = UINT_MAX;
    QMap2D_Events &events = ((EventHandler*) Table[EVENTS])->Events;

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        // Event to use temporarily to fill in data
        Event event;
        event.ServiceID   = eit->ServiceID();
        event.TableID     = eit->TableID();
        event.TransportID = eit->TSID();
        event.NetworkID   = eit->OriginalNetworkID();
        event.EventID     = eit->EventID(i);
        event.StartTime   = MythUTCToLocal(eit->StartTimeUTC(i));
        event.EndTime     = event.StartTime.addSecs(eit->DurationInSeconds(i));

#ifdef EIT_DEBUG_SID
        if (event.ServiceID == EIT_DEBUG_SID)
        {
            VERBOSE(VB_EIT, "SIParser: DVB Events: " +
                    QString("ServiceID %1 EventID: %2   Time: %3 - %4")
                    .arg(event.ServiceID).arg(event.EventID)
                    .arg(event.StartTime.toString(QString("MM/dd hh:mm")))
                    .arg(event.EndTime.toString(QString("hh:mm"))));
        }
#endif

        // Hold short & extended event information from descriptors.
        const unsigned char *bestDescriptorSE = NULL;
        vector<const unsigned char*> bestDescriptorsEE;

        // Parse descriptors
        desc_list_t list = MPEGDescriptor::Parse(
            eit->Descriptors(i), eit->DescriptorsLength(i));

        for (uint j = 0; j < list.size(); j++)
        { 
            // Pick out EIT descriptors for later parsing, and parse others.
            ProcessDVBEventDescriptors(
                0 /* pid */, list[j],
                bestPrioritySE, bestDescriptorSE,
                bestPriorityEE, bestDescriptorsEE, event);
        }

        // Parse extended event descriptions for the most preferred language
        for (uint j = 0; j < bestDescriptorsEE.size(); j++)
        {
            if (!bestDescriptorsEE[j])
            {
                event.Description = "";
                break;
            }

            ExtendedEventDescriptor eed(bestDescriptorsEE[j]);
            event.Description += eed.Text();
        }

        // Parse short event descriptor for the most preferred language
        if (bestDescriptorSE)
        {
            ShortEventDescriptor sed(bestDescriptorSE);
            event.LanguageCode    = sed.CanonicalLanguageString();
            event.Event_Name      = sed.EventName();
            event.Event_Subtitle  = sed.Text();
            if (event.Event_Subtitle == event.Event_Name)
                event.Event_Subtitle = "";
        }

        eitfixup.Fix(event, PrivateTypes.EITFixUp);

#ifdef EIT_DEBUG_SID
        if (event.ServiceID == EIT_DEBUG_SID)
        {
            VERBOSE(VB_EIT, "SIParser: DVB Events: " +
                    QString("LanguageCode='%1' "
                            "\n\t\t\tEvent_Name='%2' Description='%3'")
                    .arg(event.LanguageCode).arg(event.Event_Name)
                    .arg(event.Description));
        }
#endif
        events[eit->ServiceID()][eit->EventID(i)] = event;
    }
#endif //USING_DVB_EIT
}

/*------------------------------------------------------------------------
 *   DVB DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/

#ifdef USING_DVB_EIT
/** \fn SIParser::ProcessDVBEventDescriptors(uint,const unsigned char*,const unsigned char*,uint,vector<const unsigned char*>&,Event&)
 *  \brief Processes non-language dependent DVB Event descriptors, and caches
 *         language dependent DVB Event descriptors for the most preferred
 *         language.
 * \returns descriptor length + 2, aka total descriptor size
 */
uint SIParser::ProcessDVBEventDescriptors(
    uint                         pid,
    const unsigned char          *data,
    uint                         &bestPrioritySE,
    const unsigned char*         &bestDescriptorSE, 
    uint                         &bestPriorityEE,
    vector<const unsigned char*> &bestDescriptorsEE,
    Event                        &event)
{
    uint    descriptorTag    = data[0];
    uint    descriptorLength = data[1];
    uint    descCompression  = (event.TableID > 0x80) ? 2 : 1;

    switch (descriptorTag)
    {
        case DescriptorID::short_event:
        {
            ShortEventDescriptor sed(data);
            QString language = sed.CanonicalLanguageString();
            uint    priority = GetLanguagePriority(language);
            bestPrioritySE   = min(bestPrioritySE, priority);

            // add the descriptor, and update the language
            if (priority == bestPrioritySE)
                bestDescriptorSE = data;
        }
        break;

        case DescriptorID::extended_event:
        {
            ExtendedEventDescriptor eed(data);

            uint last_desc_number = eed.LastNumber();
            uint desc_number      = eed.DescriptorNumber();

            // Break if the descriptor is out of bounds
            if (desc_number > last_desc_number)
                break;

            QString language = eed.CanonicalLanguageString();
            uint    priority = GetLanguagePriority(language);

            // lower priority number is a higher priority...
            if (priority < bestPriorityEE)
            {
                // found a language with better priority
                // don't keep things from the wrong language
                bestDescriptorsEE.clear();
                bestPriorityEE = priority;
            }

            if (priority == bestPriorityEE)
            {
                // make sure the vector is big enough
                bestDescriptorsEE.resize(last_desc_number + 1);
                // add the descriptor, and update the language
                bestDescriptorsEE[desc_number] = data;
            }
        }
        break;

        case DescriptorID::component:
        {
            ComponentDescriptor component(data);
            event.HDTV      |= component.IsHDTV();
            event.Stereo    |= component.IsStereo();
            event.SubTitled |= component.IsReallySubtitled();
        }
        break;

        case DescriptorID::content:
        {
            ContentDescriptor content(data);
            event.ContentDescription = content.GetDescription(0);
            event.CategoryType       = content.GetMythCategory(0);
        }
        break;

        case DescriptorID::dish_event_name:
        {
            DishEventNameDescriptor dend(data);
            if (dend.HasName())
                event.Event_Name = dend.Name(descCompression);
        }
        break;

        case DescriptorID::dish_event_description:
        {
            DishEventDescriptionDescriptor dedd(data);
            if (dedd.HasDescription())
                event.Description = dedd.Description(descCompression);
        }
        break;

        default:
            CountUnusedDescriptors(pid, data);
            break;
    }
    return descriptorLength + 2;
}
#endif //USING_DVB_EIT

/*------------------------------------------------------------------------
 *   ATSC TABLE PARSERS
 *------------------------------------------------------------------------*/

/*
 *  ATSC Table 0xC7 - Master Guide Table - PID 0x1FFB
 */
void SIParser::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleMGT()");
    if (SI_STANDARD_AUTO == SIStandard)
    {
        SIStandard = SI_STANDARD_ATSC;
        VERBOSE(VB_SIPARSER, LOC + "SI Standard Detected: ATSC");
        standardChange = true;
    }

    for (uint i = 0 ; i < mgt->TableCount(); i++)
    {
        const int table_class = mgt->TableClass(i);
        const uint pid        = mgt->TablePID(i);
        QString msg = QString(" on PID 0x%2").arg(pid,0,16);
        if (table_class == TableClass::TVCTc)
        {
            TableSourcePIDs.ServicesPID   = pid;
            TableSourcePIDs.ServicesMask  = 0xFF;
            TableSourcePIDs.ServicesTable = TableID::TVCT;
            VERBOSE(VB_SIPARSER, LOC + "TVCT" + msg);
        }
        else if (table_class == TableClass::CVCTc)
        {
            TableSourcePIDs.ServicesPID   = pid;
            TableSourcePIDs.ServicesMask  = 0xFF;
            TableSourcePIDs.ServicesTable = TableID::CVCT;
            VERBOSE(VB_SIPARSER, LOC + "CVCT" + msg);
        }
        else if (table_class == TableClass::ETTc)
        {
            TableSourcePIDs.ChannelETT = pid;
            VERBOSE(VB_SIPARSER, LOC + "Channel ETT" + msg);
        }
#ifdef USING_DVB_EIT
        else if (table_class == TableClass::EIT)
        {
            const uint num = mgt->TableType(i) - 0x100;
            VERBOSE(VB_SIPARSER, LOC + QString("EIT-%1").arg(num,2) + msg);
            Table[EVENTS]->AddPid(pid, 0xCB, 0xFF, num);
        }
        else if (table_class == TableClass::ETTe)
        {
            const uint num = mgt->TableType(i) - 0x200;
            VERBOSE(VB_SIPARSER, LOC + QString("ETT-%1").arg(num,2) + msg);
            Table[EVENTS]->AddPid(pid, 0xCC, 0xFF, num);
        }
#endif // USING_DVB_EIT
        else
        {
            VERBOSE(VB_SIPARSER, LOC + "Unused Table " +
                    mgt->TableClassString(i) + msg);
        }                    
    }

#ifdef USING_DVB_EIT
    Table[EVENTS]->DependencyMet(MGT);
#endif // USING_DVB_EIT
    Table[SERVICES]->DependencyMet(MGT);
}

void SIParser::HandleVCT(uint pid, const VirtualChannelTable *vct)
{
    VERBOSE(VB_SIPARSER, LOC + "HandleVCT("<<pid<<") cnt("
            <<vct->ChannelCount()<<")");

    emit TableLoaded();

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
            Table[EVENTS]->RequestEmit(s.ATSCSourceID);
        }
        else
        {
            VERBOSE(VB_EIT, LOC + "ATSC chan "<<vct->MajorChannel(chan_idx)
                    <<"-"<<vct->MinorChannel(chan_idx)<<" is hidden in guide");
        }
#endif

        ((ServiceHandler*) Table[SERVICES])->Services[0][s.ServiceID] = s;
    }

#ifdef USING_DVB_EIT
    Table[EVENTS]->DependencyMet(SERVICES);
#endif // USING_DVB_EIT

    emit FindServicesComplete();
}

/*
 *  ATSC Table 0xCB - Event Information Table - PID Varies
 */
void SIParser::HandleEIT(uint pid, const EventInformationTable *eit)
{
    (void) pid;
    (void) eit;

#ifdef USING_DVB_EIT
    int atsc_src_id = sourceid_to_channel[eit->SourceID()];
    if (!atsc_src_id)
    {
        VERBOSE(VB_EIT, LOC +
                QString("HandleEIT(%1, sect %2, src %3): "
                        "Ignoring data. Source not in map.")
                .arg(pid).arg(eit->Section()).arg(eit->SourceID()));
        return;
    }
    else
    {
        VERBOSE(VB_EIT, LOC +
                QString("HandleEIT(%1, sect %2, src %3, ver %4): "
                        "Adding data. ATSC Channel is %5_%6.")
                .arg(pid).arg(eit->Section()).arg(eit->SourceID())
                .arg(eit->Version())
                .arg(atsc_src_id >> 8).arg(atsc_src_id & 0xff));
    }   

    EventHandler *eh = (EventHandler*) Table[EVENTS];
    QMap_Events  &ev = eh->Events[eit->SourceID()];

    for (uint i = 0; i < eit->EventCount(); i++)
    {
        Event e;
        e.SourcePID    = pid;
        e.StartTime    = MythUTCToLocal(eit->StartTimeGPS(i).addSecs(-13));
        e.EndTime      = e.StartTime.addSecs(eit->LengthInSeconds(i));
        e.ETM_Location = eit->ETMLocation(i);
        e.ServiceID    = atsc_src_id;
        e.ATSC         = true;
        e.Event_Name   = eit->title(i).CompressedString(0,0);

        desc_list_t list = MPEGDescriptor::Parse(
            eit->Descriptors(i), eit->DescriptorsLength(i));
        
        desc_list_t::const_iterator it;
        for (it = list.begin(); it != list.end(); ++it)
        {
            if (DescriptorID::caption_service == (*it)[0])
            {
                e.SubTitled = true;
#ifdef EIT_DEBUG_SID
                CaptionServiceDescriptor csd(*it);
                VERBOSE(VB_EIT, LOC + csd.toString());
#endif // EIT_DEBUG_SID
            }
            else if (DescriptorID::content_advisory == (*it)[0])
            {
                //ContentAdvisoryDescriptor cad(*it);
                // ..
            }
            else if (DescriptorID::audio_stream == (*it)[0])
            {
#ifdef EIT_DEBUG_SID
                AudioStreamDescriptor asd(*it);
                VERBOSE(VB_EIT, LOC + asd.toString());
#endif // EIT_DEBUG_SID
            }
            else
            {
                CountUnusedDescriptors(pid, *it);
            }
        }

#ifdef EIT_DEBUG_SID
        VERBOSE(VB_EIT, LOC + "HandleEIT(): " +
                QString("[%1][%2]: %3\t%4 - %5")
                .arg(atsc_src_id).arg(eit->EventID(i))
                .arg(e.Event_Name, 20)
                .arg(e.StartTime.toString("MM/dd hh:mm"))
                .arg(e.EndTime.toString("hh:mm")));
#endif // EIT_DEBUG_SID
        ev[eit->EventID(i)] = e;
    }
#endif //USING_DVB_EIT
}

/*
 *  ATSC Table 0xCC - Extended Text Table - PID Varies
 */
void SIParser::HandleETT(uint /*pid*/, const ExtendedTextTable *ett)
{
#ifdef USING_DVB_EIT
    if (!ett->IsEventETM())
        return; // decode only guide ETTs
    
    const uint atsc_src_id = sourceid_to_channel[ett->SourceID()];
    if (!atsc_src_id)
        return; // and we need atscsrcid...

    EventHandler *eh = (EventHandler*) Table[EVENTS];
    QMap_Events  &ev = eh->Events[ett->SourceID()];
    QMap_Events::iterator it = ev.find(ett->EventID());
    if (it != ev.end() && (*it).ETM_Location)
    {
        (*it).ETM_Location = 0;
        (*it).Description  = ett->ExtendedTextMessage().CompressedString(0,0);
        eitfixup.Fix(*it, PrivateTypes.EITFixUp);
    }
#endif //USING_DVB_EIT
}

/*
 *  ATSC Table 0xCD - System Time Table - PID 0x1FFB
 */
void SIParser::HandleSTT(const SystemTimeTable *stt)
{
#ifdef USING_DVB_EIT
    Table[EVENTS]->DependencyMet(STT);
#endif //USING_DVB_EIT
    VERBOSE(VB_SIPARSER, LOC + stt->toString());
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

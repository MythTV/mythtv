/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Author(s):
 *      John Pullan  (john@pullan.org)
 *      Taylor Jacob (rtjacob@earthlink.net)
 *
 * Description:
 *     Collection of classes to provide dvb a transport editor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */
#include "config.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "sitypes.h"
#include <qdeepcopy.h>

// Set EIT_DEBUG_SID to a valid serviceid to enable EIT debugging
// #define EIT_DEBUG_SID 1602

void pidHandler::reset()
{
    pid = 0;
    mask = 0;
    filter = 0;
    pulling = false;
}

void privateTypes::reset()
{
    ChannelNumbers = 0;
    Descriptors.clear();
    TVServiceTypes.clear();
    EITFixUp = 0;
    SDTMapping = false;
    CurrentTransportID = 0;
    ForceGuidePresent = false;
    CustomGuideRanges = false;
    CurrentTransportTableMin = 0;
    CurrentTransportTableMax = 0;
    OtherTransportTableMin = 0;
    OtherTransportTableMax = 0;
    GuidePID = 0;
    GuideTransportID = 0;
    CustomGuidePID = false;
    GuideOnSingleTransport = false;
    ParseSubtitleServiceIDs.clear();
}

void pullStatus::Reset()
{
    pulling = false;
    requested = false;
    requestedEmit = false;
    emitted = false;
}

void TableSourcePIDObject::Reset()
{
    TransportPID = 0;

    ServicesPID = 0;
    ServicesTable = 0;
    ServicesMask = 0;

    ChannelETT = 0;
}

void SectionTracker::Reset()
{
    MaxSections = -1;
    Version = -1;
    memset(&Filled, 0, sizeof(Filled));
}

void SectionTracker::MarkUnused(int Section)
{
    if (Section > (MaxSections+1))
        return;
    else
        Filled[Section]=2;
}

int SectionTracker::Complete()
{
    if (MaxSections == -1)
        return 0;
    for (int x = 0; x <= MaxSections; x++)
    {
        if (Filled[x] == 0)
            return 0;
    }
    return 1;
}

QString SectionTracker::loadStatus()
{
    QString retval = "";
    if (MaxSections == -1)
        return QString("[---] ");
    retval += QString("[");
    for (int x=0;x<MaxSections+1;x++)
    {
        if (Filled[x] == 1)
            retval += QString("P");
        else if (Filled[x] == 2)
            retval += QString("u");
        else
            retval += QString("m");
    }
    retval += QString("] ");
    return retval;
}

int SectionTracker::AddSection(tablehead *head)
{
    if (MaxSections == -1)
    {
         MaxSections = head->section_last;
         Version = head->version;
         Filled[head->section_number]=1;
         return 0;
    }
    else if (Version != head->version)
    {
         Reset();
         MaxSections = head->section_last;
         Version = head->version;
         Filled[head->section_number]=1;
         return -1;
    }
    else
    {
        if (Filled[head->section_number] == 1)
             return 1;
        else
        {
             Filled[head->section_number] = 1;
             return 0;
        }
    }
}

void SDTObject::Reset()
{
    ServiceID = 0;
    TransportID = 0;
    EITPresent = 0;
    NetworkID = 0;
    RunningStatus = 0;
    CAStatus = 0;
    ServiceName = "";
    ProviderName = "";
    ChanNum = -1;
    Version = 33;
    ServiceType = 0;
    ATSCSourceID = 0;
    MplexID = 0;
}

void CAPMTObject::Reset()
{
    CASystemID = 0;
    PID = 0;
    Data_Length = 0;
    memset(Data, 0, sizeof(Data));
}


Descriptor::Descriptor()
{
    Data = NULL;
    Length = 0;
}

Descriptor::Descriptor(const uint8_t *Data, const uint8_t Length)
{
    this->Length = Length;
    this->Data = new uint8_t[Length];
    memcpy(this->Data, Data, Length);
}

Descriptor::Descriptor(const Descriptor &orig)
{
    Length = orig.Length;
    Data = new uint8_t[Length];
    memcpy(Data, orig.Data, Length);
}

Descriptor::~Descriptor()
{
    if (Data)
        delete[] Data;
}

void ElementaryPIDObject::deepCopy(const ElementaryPIDObject &e)
{
    Type        = e.Type;
    Orig_Type   = e.Orig_Type;
    PID         = e.PID;
    Description = QDeepCopy<QString>(e.Description);
    Language    = QDeepCopy<QString>(e.Language);
    CA          = QDeepCopy<CAList>(e.CA);
    Record      = e.Record;

    Descriptors.clear();
    DescriptorList::const_iterator dit;
    for (dit = e.Descriptors.begin(); dit != e.Descriptors.end(); ++dit)
        Descriptors.append(Descriptor(*dit));
}

void ElementaryPIDObject::Reset()
{
    Type = ES_TYPE_UNKNOWN;
    Orig_Type = 0;
    PID = 0;
    Description = "";
    Language = "";
    CA.clear();
    Descriptors.clear();
    Record = false;
}

void Event::Reset()
{
    ServiceID = 0;
    TransportID = 0;
    NetworkID = 0;
    ATSC = false;
    clearEventValues();
}

void Event::deepCopy(const Event &e)
{
    SourcePID       = e.SourcePID;
    TransportID     = e.TransportID;
    NetworkID       = e.NetworkID;
    ServiceID       = e.ServiceID;
    EventID         = e.EventID;
    StartTime       = e.StartTime;
    EndTime         = e.EndTime;
    OriginalAirDate = e.OriginalAirDate;
    Stereo          = e.Stereo;
    HDTV            = e.HDTV;
    SubTitled       = e.SubTitled;
    ETM_Location    = e.ETM_Location;
    ATSC            = e.ATSC;
    PartNumber      = e.PartNumber;
    PartTotal       = e.PartTotal;

    // QString is not thread safe, so we must make deep copies
    LanguageCode       = QDeepCopy<QString>(e.LanguageCode);
    Event_Name         = QDeepCopy<QString>(e.Event_Name);
    Event_Subtitle     = QDeepCopy<QString>(e.Event_Subtitle);
    Description        = QDeepCopy<QString>(e.Description);
    ContentDescription = QDeepCopy<QString>(e.ContentDescription);
    Year               = QDeepCopy<QString>(e.Year);
    CategoryType       = QDeepCopy<QString>(e.CategoryType);
    Actors             = QDeepCopy<QStringList>(e.Actors);

    Credits.clear();
    QValueList<Person>::const_iterator pit = e.Credits.begin();
    for (; pit != e.Credits.end(); ++pit)
        Credits.push_back(Person(QDeepCopy<QString>((*pit).role),
                                 QDeepCopy<QString>((*pit).name)));
}

void Event::clearEventValues()
{
    SourcePID    = 0;
    LanguageCode = "";
    Event_Name   = "";
    Description  = "";
    EventID      = 0;
    ETM_Location = 0;
    Event_Subtitle     = "";
    ContentDescription = "";
    Year         = "";
    SubTitled    = false;
    Stereo       = false;
    HDTV         = false;
    ATSC         = false;
    PartNumber   = 0;
    PartTotal    = 0;
    CategoryType = "";
    OriginalAirDate = QDate();
    Credits.clear();
}

void TransportObject::Reset()
{
    Type = "";
    NetworkID = 0;
    TransportID = 0;
    Frequency = 0;
    Modulation = "auto";
    Constellation = "auto";
    SymbolRate = 0;
    FEC_Inner = "auto";
    OrbitalLocation = "";
    Polarity = "";
    FEC_Outer = "auto";
    Bandwidth = "a";
    Hiearchy = "a";
    CodeRateHP = "auto";
    CodeRateLP = "auto";
    GuardInterval = "auto";
    TransmissionMode = "a";
    Inversion = "a";
}

void NetworkObject::Reset()
{
    NetworkName = "";
    NetworkID = 0;
    LinkageTransportID = 0;
    LinkageNetworkID = 0;
    LinkageServiceID = 0;
    LinkageType = 0;
    LinkagePresent = 0;
}

/** \fn PMTObject::PMTObject(const PMTObject&)
 *  \brief Performs shallow copy.
 */
PMTObject::PMTObject(const PMTObject &other)
{
    shallowCopy(other);
}

/** \fn PMTObject::operator=(const PMTObject&)
 *  \brief Performs shallow copy.
 */
PMTObject &PMTObject::operator=(const PMTObject &other)
{
    shallowCopy(other);
    return *this;
}

/** \fn PMTObject::shallowCopy(const PMTObject&)
 *  \brief Copies each field in PMT, but shares QPtrVector data.
 */
void PMTObject::shallowCopy(const PMTObject &other)
{
    PCRPID      = other.PCRPID;
    ServiceID   = other.ServiceID;
    PMTPID      = other.PMTPID;
    CA          = other.CA;
    Descriptors = other.Descriptors;
    Components  = other.Components;
    hasCA       = other.hasCA;
    hasAudio    = other.hasAudio;
    hasVideo    = other.hasVideo;
}

/** \fn PMTObject::deepCopy(const PMTObject&)
 *  \brief Copies each simple field in PMT, and copies each element in the CA,
 *         Descriptors and Components QPtrVector fields into new QPtrVector's.
 */
void PMTObject::deepCopy(const PMTObject &other)
{
    PCRPID      = other.PCRPID;
    ServiceID   = other.ServiceID;
    PMTPID      = other.PMTPID;

    CA          = QDeepCopy<CAList>(other.CA);

    Descriptors.clear();
    DescriptorList::const_iterator dit;
    for (dit = other.Descriptors.begin(); dit!=other.Descriptors.end(); ++dit)
        Descriptors.append(Descriptor(*dit));

    Components.clear();
    QValueList<ElementaryPIDObject>::const_iterator eit;
    for (eit = other.Components.begin(); eit!=other.Components.end(); ++eit)
    {
        ElementaryPIDObject obj;
        obj.deepCopy(*eit);
        Components.append(obj);
    }

    hasCA       = other.hasCA;
    hasAudio    = other.hasAudio;
    hasVideo    = other.hasVideo;
}

/** \fn PMTObject::Reset()
 *  \brief Resets all values in the PMTObject to their initial values.
 */
void PMTObject::Reset()
{
    ServiceID = 0;
    PCRPID = 0;
    CA.clear();
    Components.clear();
    PMTPID = 0;

    hasCA = false;
    hasAudio = false;
    hasVideo = false;
}

void PATHandler::Reset()
{
    status.Reset();
    Tracker.Reset();
    pids.clear();
}

bool PATHandler::RequirePIDs()
{
    if ((status.pulling == false) && (status.requested == true))
        return true;
    return false;
}

bool PATHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    if (status.pulling == true)
        return false;
    pid = 0x00;
    filter = 0x00;
    mask = 0xFF;
    status.pulling = true;
    return true;
}

void PATHandler::Request(uint16_t key)
{
    (void) key;
    status.requested = true;
}

bool PATHandler::Complete()
{
    if (Tracker.Complete() && (!status.emitted))
    {
        if (status.requestedEmit == false)
            status.emitted = true;
        return true;
    }
    return false;
}

bool PATHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    (void) key0;
    (void) key1;
    return Tracker.AddSection(head);
}

bool PMTHandler::RequirePIDs()
{
    if (!patloaded)
        return false;

    QMap_pullStatus::Iterator i;
    for (i = status.begin() ; i != status.end() ; ++i)
    {
        if ((i.data().pulling == false) && (i.data().requested))
            return true;
    }

    return false;
}

bool PMTHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    QMap_pullStatus::Iterator i;
    for (i = status.begin() ; i != status.end() ; ++i)
    {
        if ((i.data().pulling == false) && (i.data().requested == true))
        {
            i.data().pulling = true;
            pid = pmt[i.key()].PMTPID;
            filter = 0x02;
            mask = 0xFF;
            return true;
        }
    }

    return false;
}

void PMTHandler::RequestEmit(uint16_t key)
{
    status[key].requested = true;
    status[key].requestedEmit = true;
}

bool PMTHandler::EmitRequired()
{
    QMap_pullStatus::Iterator i;
    for (i = status.begin() ; i != status.end() ; ++i)
    {
        if (i.data().requestedEmit && (i.data().emitted == false) && Tracker[i.key()].Complete())
            return true;
    }
    return false;
}

bool PMTHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    QMap_pullStatus::Iterator i;
    for (i = status.begin() ; i != status.end() ; ++i)
    {
        if ((i.data().requestedEmit) && (i.data().emitted == false) && Tracker[i.key()].Complete())
        {
            i.data().emitted = true;
            key0 = i.key();
            key1 = 0;
            return true;
        }
    }
    return false;
}

bool PMTHandler::AddSection(tablehead_t* head, uint16_t key0, uint16_t key1)
{
    (void) key0;
    (void) key1;

    return Tracker[key0].AddSection(head);
}

void PMTHandler::AddKey(uint16_t key0, uint16_t key1)
{
    (void) key1;
    Tracker[key0].Reset();
    if (!(status.contains(key0)))
        status[key0].Reset();;
}

void PMTHandler::DependencyMet(tabletypes t)
{
    if (t == PAT)
        patloaded = true;
}

void PMTHandler::DependencyChanged(tabletypes t)
{
    (void) t;
    //TODO: Handle this situation
}

bool MGTHandler::RequirePIDs()
{
    if ((status.pulling == false) && (status.requested == true))
        return true;
    return false;
}

/* It's best to open the PID wide open so you get the other ATSC tables */
bool MGTHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    if (status.pulling == true)
        return false;
    pid = 0x1FFB;
    filter = 0xFF;
    mask = 0x00;
    status.pulling = true;
    return true;
}

void MGTHandler::Request(uint16_t key)
{
    (void) key;
    status.requested = true;
}

bool MGTHandler::Complete()
{
    if (Tracker.Complete() && (!status.emitted))
    {
        if (status.requestedEmit == false)
            status.emitted = true;
        return true;
    }
    return false;
}

bool MGTHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    (void) key0;
    (void) key1;

    int retval = Tracker.AddSection(head);

    if (retval == -1)
        return false;
    return retval;
}

bool STTHandler::RequirePIDs()
{
    if ((status.pulling == false) && (status.requested == true))
        return true;
    return false;
}

bool STTHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    if (status.pulling == true)
        return false;
    pid = 0x1FFB;
    filter = 0xFF;
    mask = 0x00;
    status.pulling = true;
    return true;
}

void STTHandler::Request(uint16_t key)
{
    (void) key;
    status.requested = true;
}

bool STTHandler::Complete()
{
    if (Tracker.Complete() && (!status.emitted))
    {
        if (status.requestedEmit == false)
            status.emitted = true;
        return true;
    }
    return false;
}

bool STTHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    (void) key0;
    (void) key1;

    return Tracker.AddSection(head);
}

void EventHandler::Reset()
{
    Tracker.clear();
    mgtloaded = false;
    sttloaded = false;
    servicesloaded = false;
    EITpid.clear();
    ETTpid.clear();
    Events.clear();
    CompleteSent = false;
    SIStandard = SI_STANDARD_DVB;
}

bool EventHandler::Complete()
{
    if (status.empty())
        return false;

    if (SIStandard == SI_STANDARD_ATSC)
    {
        if (!(mgtloaded))
            return false;
        if (!(sttloaded))
            return false;
    }

    if (!(servicesloaded))
        return false;

    if (CompleteSent)
        return false;

    QMap_pullStatus::Iterator k;
    for (k = status.begin() ; k != status.end() ; ++k)
    {
        if ((k.data().pulling == true) && (k.data().emitted == true))
        {
            CompleteSent = true;
            return true;
        }
    }
    return false;
}

void EventHandler::SetupTrackers()
{
    QMap_pullStatus::Iterator s;

    if (SIStandard == SI_STANDARD_ATSC)
    {
        QMap_pidHandler::Iterator p;
        for (s = status.begin() ; s != status.end() ; ++s)
        {
            for (p = EITpid.begin() ; p != EITpid.end() ; ++p)
                Tracker[s.key()][p.key()].Reset();
            TrackerSetup[s.key()] = true;
            s.data().pulling = true;
        }
    }

    if (SIStandard == SI_STANDARD_DVB)
    {
        for (s = status.begin() ; s != status.end() ; ++s)
        {
// Temporary value for now..
            s.data().pulling = true;
            TrackerSetup[s.key()] = false;
        }
    }
}

bool EventHandler::RequirePIDs()
{
    if (SIStandard == SI_STANDARD_ATSC)
    {
        if (!(mgtloaded))
            return false;
        if (!(sttloaded))
            return false;
    }

    if (!(servicesloaded))
        return false;

    QMap_pidHandler::Iterator i;
    for (i = EITpid.begin() ; i != EITpid.end() ; ++i)
    {
        if(i.data().pulling == false)
        {
            SetupTrackers();
            return true;
        }
    }
    for (i = ETTpid.begin() ; i != ETTpid.end() ; ++i)
    {
        if(i.data().pulling == false)
        {
            SetupTrackers();
            return true;
        }
    }
    return false;
}

bool EventHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    QMap_pidHandler::Iterator i;
    for (i = EITpid.begin() ; i != EITpid.end() ; ++i)
    {
        if(i.data().pulling == false)
        {
            pid = i.data().pid;
            filter = i.data().filter;
            mask = i.data().mask;
            i.data().pulling = true;
            return true;
        }
    }
    for (i = ETTpid.begin() ; i != ETTpid.end() ; ++i)
    {
        if(i.data().pulling == false)
        {
            pid = i.data().pid;
            filter = i.data().filter;
            mask = i.data().mask;
            i.data().pulling = true;
            return true;
        }
    }
    return false;
}

void EventHandler::RequestEmit(uint16_t key)
{
#ifdef EIT_DEBUG_SID
    fprintf(stdout,"EventHandler::RequestEmit for serviceid=%i\n", key);
#endif
    status[key].requested = true;
    status[key].requestedEmit = true;
    status[key].emitted = false;
}

bool EventHandler::EmitRequired()
{
    QMap_pidHandler::Iterator p;
    QMap_SectionTracker::Iterator i;
    QMap_pullStatus::Iterator s;
    QMap_Events::Iterator e;
    bool AllComplete;

    if (SIStandard == SI_STANDARD_ATSC)
    {
        if (!(mgtloaded))
            return false;
        if (!(sttloaded))
            return false;
    }
    if (!(servicesloaded))
        return false;

    for (s = status.begin() ; s != status.end() ; ++s)
    {
        if (s.data().emitted || !s.data().pulling)
        {
            continue;
        }

        AllComplete = true;
        /* Make sure all sections are being pulled otherwise your not done */
        if (TrackerSetup[s.key()] == false)
            AllComplete = false;
        for (i = Tracker[s.key()].begin() ; i != Tracker[s.key()].end() ; ++i)
        {
            if (!i.data().Complete())
            {
                AllComplete = false;
                break;
            }
        }
        if (SIStandard == SI_STANDARD_ATSC)
        {
            for (e = Events[s.key()].begin() ; e != Events[s.key()].end() ; ++e)
            {
                if (e.data().ETM_Location != 0)
                {
                    for (p = EITpid.begin() ; p != EITpid.end() ; ++p)
                    {
                        if (e.data().SourcePID == p.data().pid)
                        {
                            /* Delete events that need ETTs that aren't filtered */
                            if (ETTpid.contains(p.key()))
                                AllComplete = false;
// Don't remove it because it screwed up the list.. This is temporary.. Use Stack like DVBSIParser
//                                    else
//                                        Events[s.key()].remove(e);
                        }
                    }
                }
            }
        }
        if (AllComplete)
            return true;
    }
    return false;
}

bool EventHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    QMap_pidHandler::Iterator p;
    QMap_SectionTracker::Iterator i;
    QMap_pullStatus::Iterator s;
    QMap_Events::Iterator e;
    bool AllComplete;

    for (s = status.begin() ; s != status.end() ; ++s)
    {
        if ((s.data().requestedEmit) && (s.data().emitted != true))
        {
            AllComplete = true;
            if (TrackerSetup[s.key()] == false)
                AllComplete = false;
            /* Make sure all sections are being pulled otherwise your not done */
            for (i = Tracker[s.key()].begin() ; i != Tracker[s.key()].end() ; ++i)
            {
                if (!(i.data().Complete()))
                    AllComplete = false;
            }
            if (SIStandard == SI_STANDARD_ATSC)
            {
                for (e = Events[s.key()].begin() ; e != Events[s.key()].end() ; ++e)
                {
                    if (e.data().ETM_Location != 0)
                    {
                        for (p = EITpid.begin() ; p != EITpid.end() ; ++p)
                        {
                            if (e.data().SourcePID == p.data().pid)
                            {
                                /* Delete events that need ETTs that aren't filtered */
                                if (ETTpid.contains(p.key()))
                                    AllComplete = false;
    // Don't remove it because it screwed up the list.. This is temporary..
    //                                    else
    //                                        Events[s.key()].remove(e);
                            }
                        }
                    }
                }
            }
            if (AllComplete)
            {
                key0 = s.key();
                key1 = 0;
                s.data().emitted = true;
                return true;
            }
        }
    }
    return false;
}

void EventHandler::DependencyMet(tabletypes t)
{
    if (t == MGT)
        mgtloaded = true;
    if (t == STT)
        sttloaded = true;
    if (t == SERVICES)
        servicesloaded = true;
}

void EventHandler::AddPid(uint16_t pid,uint8_t filter, uint8_t mask, uint8_t key)
{
    if (filter == 0xCB)
    {
        if (EITpid.contains(key))
            return;
        EITpid[key].pid = pid;
        EITpid[key].filter = filter;
        EITpid[key].mask = mask;
        EITpid[key].pulling = false;
        return;
    }
    if (filter == 0xCC)
    {
        if (ETTpid.contains(key))
            return;
        ETTpid[key].pid = pid;
        ETTpid[key].filter = filter;
        ETTpid[key].mask = mask;
        ETTpid[key].pulling = false;
        return;
    }
    if (EITpid.contains(key))
        return;
    EITpid[key].pid = pid;
    EITpid[key].filter = filter;
    EITpid[key].mask = mask;
    EITpid[key].pulling = false;
}

bool EventHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    int retval = false;

#ifdef EIT_DEBUG_SID
    if (key0 == EIT_DEBUG_SID) {
        printf("EventHandler::AddSection sid=%i eid=%i version=%i section %02x of %02x\n", key0, key1, head->version, head->section_number, head->section_last);
    }
#endif

    if (SIStandard == SI_STANDARD_ATSC)
    {
        QMap_pidHandler::Iterator p;
        uint16_t realkey1 = 1000;
        for (p = EITpid.begin() ; p != EITpid.end() ; ++p)
        {
            if (p.data().pid == key1)
                realkey1 = p.key();
        }
        retval = Tracker[key0][realkey1].AddSection(head);
    }

    if (SIStandard == SI_STANDARD_DVB)
    {
#ifdef EIT_DEBUG_SID
        if (key0 == EIT_DEBUG_SID) {
            QString foo = Tracker[key0][key1].loadStatus();
            printf("%s\n", foo.ascii());
        }
#endif
        return Tracker[key0][key1].AddSection(head);
    }

    if (retval == 1)
        return true;

    if (retval == -1)
    {
       retval = -1;   // Here you will reset the trackers and allow an emit again
    }
    return false;
}

void ServiceHandler::Reset()
{
    Tracker.clear();
    mgtloaded = false;
    nitloaded = false;
    status.clear();
    CompleteSent = false;
    Services.clear();
    SIStandard = SI_STANDARD_AUTO;
}

bool ServiceHandler::Complete()
{
    if (status.empty())
        return false;

    if ((SIStandard == SI_STANDARD_ATSC) && (!(mgtloaded)) )
        return false;

    if ((SIStandard == SI_STANDARD_DVB) && (!(nitloaded)) )
        return false;

    if (CompleteSent)
        return false;

    QMap_pullStatus::Iterator k;
    for (k = status.begin() ; k != status.end() ; ++k)
    {
        if ((k.data().pulling == true) && (k.data().emitted == true))
        {
            CompleteSent = true;
            return true;
        }
    }
    return false;
}

bool ServiceHandler::RequirePIDs()
{
    if (SIStandard == SI_STANDARD_AUTO)
        return false;

    if ((SIStandard == SI_STANDARD_ATSC) && (!(mgtloaded)) )
        return false;

    if ((SIStandard == SI_STANDARD_DVB) && (!(nitloaded)) )
        return false;

    return (!(status[0].pulling));
}

bool ServiceHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    if ((SIStandard == SI_STANDARD_DVB) && (status[0].pulling == false))
    {
        pid = 0x11;
        filter = 0x46;
        mask = 0xFB;
        status[0].pulling = true;
        return true;
    }
    if ((SIStandard == SI_STANDARD_ATSC) && (status[0].pulling == false))
    {
        pid = 0x1FFB;
        filter = 0xFF;
        mask = 0x00;
        status[0].pulling = true;
        return true;
    }
    return false;
}

void ServiceHandler::RequestEmit(uint16_t key)
{
    status[key].requested = true;
    status[key].requestedEmit = true;
    status[key].emitted = false;
}

void ServiceHandler::Request(uint16_t key)
{
    status[key].requested = true;
    status[key].requestedEmit = false;
    status[key].emitted = false;
}

bool ServiceHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    key0 = 0;
    key1 = 0;
    return false;
}

void ServiceHandler::DependencyMet(tabletypes t)
{
    if (t == MGT)
        mgtloaded = true;
    if (t == NETWORK)
        nitloaded = true;

}

bool ServiceHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
   (void) key1;
   return Tracker[key0].AddSection(head);
}

void NetworkHandler::Reset()
{
    Tracker.Reset();
    status.Reset();
    CompleteSent = false;
    NITList.Network.clear();
    NITList.Transport.clear();
    SIStandard = SI_STANDARD_AUTO;
}

bool NetworkHandler::Complete()
{
    if (SIStandard == SI_STANDARD_ATSC)
        return false;

    if (CompleteSent)
        return false;

    if ((status.pulling == true) && (Tracker.Complete()) )
    {
        CompleteSent = true;
        return true;
    }
    return false;
}

bool NetworkHandler::RequirePIDs()
{
    if (SIStandard == SI_STANDARD_ATSC)
        return false;

    if ((status.pulling == false) && (status.requested == true))
        return true;

    return false;
}

bool NetworkHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    if ((SIStandard == SI_STANDARD_DVB) && (status.pulling == false))
    {
        pid = 0x10;
        filter = 0x40;
        mask = 0xFF;
        status.pulling = true;
        return true;
    }
    return false;
}

void NetworkHandler::Request(uint16_t key)
{
    (void) key;
    status.requested = true;
}

void NetworkHandler::RequestEmit(uint16_t key)
{
    (void) key;
    status.requested = true;
    status.requestedEmit = true;
    status.emitted = false;
}

bool NetworkHandler::EmitRequired()
{
    if ((status.emitted == false) && (Tracker.Complete()) && (status.requestedEmit))
        return true;
    return false;
}

bool NetworkHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    if (status.emitted == false)
    {
        key0 = 0;
        key1 = 0;
        status.emitted = true;
        return true;
    }
    return false;
}

bool NetworkHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    (void) head;
    (void) key1;
    (void) key0;
    return Tracker.AddSection(head);
}

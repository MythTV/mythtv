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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <qdeepcopy.h>

#include "sitypes.h"
#include "mythconfig.h"
#include "mythcontext.h"

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

void SectionTracker::Reset(void)
{
    maxSections = -1;
    version     = -1;
    empty       = true;
    fillStatus.clear();
}

void SectionTracker::MarkUnused(int section)
{
    if (section > maxSections)
        return;
    
    fillStatus[section] = kStatusUnused;
}

bool SectionTracker::IsComplete(void) const
{
    if (maxSections < 0)
        return false;

    for (int x = 0; x <= maxSections; x++)
    {
        if (kStatusEmpty == fillStatus[x])
            return false;
    }

    return true;
}

QString SectionTracker::Status(void) const
{
    if (maxSections < 0)
        return QString("[---] ");

    QString retval = "[";
    for (int x = 0; x <= maxSections; x++)
    {
        switch (fillStatus[x])
        {
            case kStatusEmpty:  retval += "m"; break;
            case kStatusFilled: retval += "P"; break;
            case kStatusUnused: retval += "u"; break;
            default:            retval += "i"; break;
        }
    }

    return retval + "] ";
}

int SectionTracker::AddSection(const tablehead *head)
{
    empty = false;
    if (maxSections < 0)
    {
         maxSections = head->section_last;
         version     = head->version;

         fillStatus.clear();
         fillStatus.resize(maxSections+10, kStatusEmpty);
         fillStatus[head->section_number] = kStatusFilled;
         return 0;
    }
    else if (version != head->version)
    {
         Reset();
         maxSections = head->section_last;
         version     = head->version;

         fillStatus.clear();
         fillStatus.resize(maxSections+10, kStatusEmpty);
         fillStatus[head->section_number] = kStatusFilled;
         return -1;
    }
    else if (head->section_number <= maxSections)
    {
        if (kStatusFilled == fillStatus[head->section_number])
             return 1;
        else
        {
             fillStatus[head->section_number] = kStatusFilled;
             return 0;
        }
    }
    return 1; // just ignore it if it is outside bounds
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

ElementaryPIDObject::ElementaryPIDObject(const ElementaryPIDObject &other)
{
    deepCopy(other);
}

void ElementaryPIDObject::deepCopy(const ElementaryPIDObject &e)
{
    Orig_Type   = e.Orig_Type;
    PID         = e.PID;
    CA          = QDeepCopy<CAList>(e.CA);
    Record      = e.Record;

    desc_list_t::iterator it = Descriptors.begin();
    for (; it != Descriptors.end(); ++it)
        delete [] *it;
    Descriptors.clear();

    desc_list_t::const_iterator cit = e.Descriptors.begin();
    for (; cit != e.Descriptors.end(); ++cit)
    {
        unsigned char *tmp = new unsigned char[(*cit)[1] + 2];
        memcpy(tmp, *cit, (*cit)[1] + 2);
        Descriptors.push_back(tmp);
    }
}

void ElementaryPIDObject::Reset()
{
    Orig_Type = 0;
    PID = 0;
    CA.clear();

    desc_list_t::iterator it = Descriptors.begin();
    for (; it != Descriptors.end(); ++it)
        delete [] *it;
    Descriptors.clear();

    Record = false;
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
 *  \brief Performs deep copy.
 */
PMTObject::PMTObject(const PMTObject &other)
{
    deepCopy(other);
}

/** \fn PMTObject::operator=(const PMTObject&)
 *  \brief Performs deep copy.
 */
PMTObject &PMTObject::operator=(const PMTObject &other)
{
    deepCopy(other);
    return *this;
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

    desc_list_t::iterator it = Descriptors.begin();
    for (; it != Descriptors.end(); ++it)
        delete [] *it;
    Descriptors.clear();

    desc_list_t::const_iterator cit = other.Descriptors.begin();
    for (; cit != other.Descriptors.end(); ++cit)
    {
        unsigned char *tmp = new unsigned char[(*cit)[1] + 2];
        memcpy(tmp, *cit, (*cit)[1] + 2);
        Descriptors.push_back(tmp);
    }

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
    PCRPID    = 0;
    ServiceID = 0;
    PMTPID    = 0;
    CA.clear();

    desc_list_t::iterator it = Descriptors.begin();
    for (; it != Descriptors.end(); ++it)
        delete [] *it;
    Descriptors.clear();
    Components.clear();

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
    if (Tracker.IsComplete() && (!status.emitted))
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
        if (i.data().requestedEmit && !i.data().emitted &&
            Tracker[i.key()].IsComplete())
        {
            return true;
        }
    }
    return false;
}

bool PMTHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    QMap_pullStatus::Iterator i;
    for (i = status.begin() ; i != status.end() ; ++i)
    {
        if (i.data().requestedEmit && !i.data().emitted &&
            Tracker[i.key()].IsComplete())
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
    if (Tracker.IsComplete() && !status.emitted)
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

#ifdef USING_DVB_EIT
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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
bool EventHandler::Complete()
{
    if (status.empty())
        return false;

    if (SIStandard == SI_STANDARD_ATSC && (!mgtloaded || !sttloaded))
        return false;

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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
bool EventHandler::RequirePIDs()
{
    if (SIStandard == SI_STANDARD_ATSC && (!mgtloaded || !sttloaded))
        return false;

    if (!(servicesloaded))
        return false;

    QMap_pidHandler::Iterator i;
    for (i = EITpid.begin() ; i != EITpid.end() ; ++i)
    {
        if (i.data().pulling == false)
        {
            SetupTrackers();
            return true;
        }
    }
    for (i = ETTpid.begin() ; i != ETTpid.end() ; ++i)
    {
        if (i.data().pulling == false)
        {
            SetupTrackers();
            return true;
        }
    }
    return false;
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
bool EventHandler::GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask)
{
    QMap_pidHandler::Iterator i;
    for (i = EITpid.begin() ; i != EITpid.end() ; ++i)
    {
        if (i.data().pulling == false)
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
        if (i.data().pulling == false)
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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
void EventHandler::RequestEmit(uint16_t key)
{
#ifdef EIT_DEBUG_SID
    VERBOSE(VB_EIT, QString("EventHandler::RequestEmit for serviceid = %1")
            .arg(key));
#endif

    status[key].requested = true;
    status[key].requestedEmit = true;
    status[key].emitted = false;
// HACK begin -- this is probably not the right place to set this
    status[key].pulling = true;
// HACK end
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
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
    {
#ifdef EIT_DEBUG_SID
        static int n =0;
        if (!(n++%100))
            VERBOSE(VB_EIT, "EventHandler::EmitRequired no services");
#endif
        return false;
    }

    for (s = status.begin() ; s != status.end() ; ++s)
    {
#ifdef EIT_DEBUG_SID
        if (s.key() == EIT_DEBUG_SID &&
            (s.data().emitted || !s.data().pulling))
        {
            VERBOSE(VB_EIT, QString("EventHandler::EmitRequired %1: "
                                    "not pulling").arg(EIT_DEBUG_SID));
        }
#endif
        if (s.data().emitted || !s.data().pulling)
            continue;

        if (!TrackerSetup[s.key()])
            continue;

        AllComplete = true;
        /* Make sure all sections are being pulled otherwise your not done */
        for (i = Tracker[s.key()].begin() ; i != Tracker[s.key()].end() ; ++i)
        {
            if (!i.data().IsComplete())
            {
#ifdef EIT_DEBUG_SID
                if (s.key() == EIT_DEBUG_SID)
                {
                    VERBOSE(VB_EIT, QString("EventHandler::EmitRequired %1:"
                                            " 0x%2 is not complete")
                            .arg(EIT_DEBUG_SID).arg(i.key(),0,16));
                }
#endif
                AllComplete = false;
                break;
            }
        }
        if (AllComplete && (SIStandard == SI_STANDARD_ATSC))
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
#ifdef EIT_DEBUG_SID
        if (s.key() == EIT_DEBUG_SID)
        {
            VERBOSE(VB_EIT, "EventHandler::EmitRequired (End) "
                    <<EIT_DEBUG_SID<<": AllComplete = "<<AllComplete);
        }
#endif
        if (AllComplete)
            return true;
    }
    return false;
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
bool EventHandler::GetEmitID(uint16_t& key0, uint16_t& key1)
{
    QMap_pidHandler::Iterator p;
    QMap_pullStatus::Iterator s;
    QMap_Events::Iterator e;
    bool AllComplete;

    for (s = status.begin() ; s != status.end() ; ++s)
    {
        if ((s.data().requestedEmit) && (s.data().emitted != true))
        {
            AllComplete = true;
            if (TrackerSetup[s.key()] == false)
                continue;

            // Make sure all sections are being pulled,
            // otherwise your not done.
            QMap_SectionTracker::Iterator it = Tracker[s.key()].begin();
            while (it != Tracker[s.key()].end())
            {
                if (!(it.data().IsComplete()))
                {
                    AllComplete = false;
                    break;
                }
                ++it;
            }

            if (AllComplete && (SIStandard == SI_STANDARD_ATSC))
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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
void EventHandler::DependencyMet(tabletypes t)
{
    mgtloaded      |= (t == MGT);
    sttloaded      |= (t == STT);
    servicesloaded |= (t == SERVICES);
}
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
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
#endif //USING_DVB_EIT

#ifdef USING_DVB_EIT
bool EventHandler::AddSection(tablehead_t *head, uint16_t key0, uint16_t key1)
{
    int retval = false;

#ifdef EIT_DEBUG_SID
    if (key0 == EIT_DEBUG_SID)
    {
        VERBOSE(VB_EIT, QString("EventHandler::AddSection sid(%1) eid(%2) "
                                "version(%3) section 0x%4 of %0x%5")
                .arg(key0).arg(key1).arg(head->version)
                .arg(head->section_number,0,16).arg(head->section_last,0,16));
    }
#endif

// HACK begin -- this is probably not the right place to set this
    // If we are interested in this but the tracker does not have any
    // data in it then set the status back to pulling if appropriate.
    if (status[key0].requestedEmit && status[key0].emitted &&
        Tracker[key0][key1].IsEmpty())
    {
        status[key0].pulling = true;
        status[key0].emitted = false;
    }
// HACK end

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
        if (key0 == EIT_DEBUG_SID)
        {
            QString foo = Tracker[key0][key1].Status();
            VERBOSE(VB_EIT, "EventHandler::AddSection Status: "<<foo);
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
#endif //USING_DVB_EIT

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

    if (status.pulling && Tracker.IsComplete())
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
    return !status.emitted && status.requestedEmit && Tracker.IsComplete();
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

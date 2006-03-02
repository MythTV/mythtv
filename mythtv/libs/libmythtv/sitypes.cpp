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
    nitloaded |= (t == NETWORK);
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

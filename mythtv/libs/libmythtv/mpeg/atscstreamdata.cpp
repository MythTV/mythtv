// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "atscstreamdata.h"
#include "atsctables.h"
#include "RingBuffer.h"
#include "hdtvrecorder.h"

/** \fn ATSCStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo All RRT tables are ignored
 *  \todo We don't check the start time of EIT and ETT tables
 *        in the version check, so many tables are improperly
 *        ignored.
 */
bool ATSCStreamData::IsRedundant(const PSIPTable &psip) const
{
    const int table_id = psip.TableID();
    const int version = psip.Version();

    if (TableID::PAT == table_id)
        return (version == VersionPAT());

    if (TableID::PMT == table_id)
        return (version == VersionPMT());

    if (TableID::MGT == table_id && VersionMGT() == version)
        return true; // no need to resync stream

    if (TableID::TVCT == table_id)
    {
        TerrestrialVirtualChannelTable tvct(psip);
        if (VersionTVCT(tvct.TransportStreamID()) == version)
            return true; // no need to resync stream
    }

    if (TableID::CVCT == table_id)
    {
        CableVirtualChannelTable cvct(psip);
        if (VersionCVCT(cvct.TransportStreamID()) == version)
            return true; // no need to resync stream
    }

    if (TableID::RRT == table_id)
        return true; // we ignore RatingRegionTables

    if (TableID::STT == table_id)
        return false; // each SystemTimeTable matters

    if ((TableID::EIT == table_id) && (VersionEIT(psip.tsheader()->PID()) == version))
    {
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return true; // no need to resync stream
    }

    if ((TableID::ETT == table_id) && (VersionETT(psip.tsheader()->PID()) == version))
    {
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return true; // no need to resync stream
    }

    return false;
}

void ATSCStreamData::HandleTables(const TSPacket* tspacket)
{
#define HT_RETURN { delete psip; return; }
    // Assemble PSIP
    PSIPTable *psip = AssemblePSIP(tspacket);
    if (!psip)
        return;
    const int version = psip->Version();

    // Validate PSIP
    if (!psip->IsGood())
    {
        VERBOSE(VB_RECORD, QString("PSIP packet failed CRC check"));
        HT_RETURN;
    }

    if (!psip->IsCurrent()) // we don't cache the next table, for now
        HT_RETURN;

    if (1!=tspacket->AdaptationFieldControl())
    { // payload only, ATSC req.
        VERBOSE(VB_RECORD,
                "PSIP packet has Adaptation Field Control, not ATSC compiant");
        HT_RETURN;
    }

    if (tspacket->ScramplingControl())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        VERBOSE(VB_RECORD,
                "PSIP packet is scrambled, not ATSC/DVB compiant");
        HT_RETURN;
    }

    // Don't decode redundant packets,
    // but if it is a PAT or PMT emit a "heartbeat" signal.
    if (IsRedundant(*psip))
    {
        if (TableID::PAT == psip->TableID())
            emit UpdatePAT(PAT());
        if (TableID::PMT == psip->TableID())
            emit UpdatePMT(PMT());
        HT_RETURN; // already parsed this table, toss it.
    }

    // If we get this far decode any PAT, PMT or MGT...
    switch (psip->TableID())
    {
        case TableID::PAT:
        {
            if (CreatePAT(ProgramAssociationTable(*psip)))
                emit UpdatePAT(PAT());
            HT_RETURN;
        }
        case TableID::PMT:
        {
            if (CreatePMT(ProgramMapTable(*psip)))
                emit UpdatePMT(PMT());
            HT_RETURN;
        }
        case TableID::MGT:
        {
            SetVersionMGT(version);
            MasterGuideTable mgt(*psip);
            emit UpdateMGT(&mgt);
            HT_RETURN;
        }
    }

    // Don't decode any non-PAT/PMT/MGT tables if we haven't send an MGT.
    if (VersionMGT()<0)
        HT_RETURN;

    // Decode any tables we know about.
    switch (psip->TableID())
    {
        case TableID::TVCT:
        {
            TerrestrialVirtualChannelTable vct(*psip);
            SetVersionTVCT(vct.TransportStreamID(), version);
            emit UpdateTVCT(vct.TransportStreamID(), &vct);
            emit UpdateVCT(vct.TransportStreamID(), &vct);
            break;
        }
        case TableID::CVCT:
        {
            CableVirtualChannelTable vct(*psip);
            SetVersionCVCT(vct.TransportStreamID(), version);
            emit UpdateCVCT(vct.TransportStreamID(), &vct);
            emit UpdateVCT(vct.TransportStreamID(), &vct);
            break;
        }
        case TableID::RRT:
        {
            RatingRegionTable rrt(*psip);
            emit UpdateRRT(&rrt);
            break;
        }
        case TableID::EIT:
        {
            EventInformationTable eit(*psip);
            SetVersionEIT(tspacket->PID(), version);
            emit UpdateEIT(tspacket->PID(), &eit);
            break;
        }
        case TableID::ETT:
        {
            ExtendedTextTable ett(*psip);
            SetVersionETT(tspacket->PID(), version);
            emit UpdateETT(tspacket->PID(), &ett);
            break;
        }
        case TableID::STT:
        {
            SystemTimeTable stt(*psip);
            if (stt.GPSOffset() != _GPS_UTC_offset) // only update if it changes
                _GPS_UTC_offset = stt.GPSOffset();
            emit UpdateSTT(&stt);
            break;
        }
        case TableID::DCCT:
        {
            DirectedChannelChangeTable dcct(*psip);
            emit UpdateDCCT(&dcct);
            break;
        }
        case TableID::DCCSCT:
        {
            DirectedChannelChangeSelectionCodeTable dccsct(*psip);
            emit UpdateDCCSCT(&dccsct);
            break;
        }
        default:
        {
            VERBOSE(VB_RECORD, QString("Unknown table 0x%1")
                    .arg(psip->TableID(),0,16));
            break;
        }
    }
    HT_RETURN;
#undef HT_RETURN
}

void ATSCStreamData::PrintMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_RECORD, mgt->toString());
}

void ATSCStreamData::PrintSTT(const SystemTimeTable *stt)
{
    VERBOSE(VB_RECORD, stt->toString());
}

void ATSCStreamData::PrintRRT(const RatingRegionTable*)
{
    VERBOSE(VB_RECORD, QString("RRT"));
}

void ATSCStreamData::PrintDCCT(const DirectedChannelChangeTable*)
{
    VERBOSE(VB_RECORD, QString("DCCT"));
}

void ATSCStreamData::PrintDCCSCT(
    const DirectedChannelChangeSelectionCodeTable*)
{
    VERBOSE(VB_RECORD, QString("DCCSCT"));
}

void ATSCStreamData::PrintTVCT(
    uint /*pid*/, const TerrestrialVirtualChannelTable* tvct)
{
    VERBOSE(VB_RECORD, tvct->toString());
}

void ATSCStreamData::PrintCVCT(
    uint /*pid*/, const CableVirtualChannelTable* cvct)
{
    VERBOSE(VB_RECORD, cvct->toString());
}

void ATSCStreamData::PrintEIT(uint pid, const EventInformationTable *eit)
{
    VERBOSE(VB_RECORD, QString("EIT pid(%1) %2")
            .arg(pid).arg(eit->toString()));
}

void ATSCStreamData::PrintETT(uint pid, const ExtendedTextTable *ett)
{
    VERBOSE(VB_RECORD, QString("ETT pid(%1) %2")
            .arg(pid).arg(ett->toString()));
}

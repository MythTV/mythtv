// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "atscstreamdata.h"
#include "atsctables.h"
#include "RingBuffer.h"
#include "hdtvrecorder.h"

// returns true if table already seen
bool ATSCStreamData::IsRedundant(const PSIPTable &psip) const {
    const int table_id = psip.TableID();
    const int version = psip.Version();

    if (0x00==table_id)
        return (version==VersionPAT());

    if (0x02==table_id)
        return (version==VersionPMT());

    if (0xC7==table_id && VersionMGT()==version)
        return true; // no need to resync stream

    if (0xC8==table_id) { // VCT Terra
        TerrestrialVirtualChannelTable tvct(psip);
        if (VersionTVCT(tvct.TransportStreamID())==version)
            return true; // no need to resync stream
    }

    if (0xCA==table_id) // RRT, ignore
        return true; // no need to resync stream

    if (0xCD==table_id) // STT, each one matters
        return false;

    //cerr<<hex<<"0x"<<table_id<<dec<<endl;
    if ((0xCB==table_id) && (VersionEIT(psip.tsheader()->PID())==version)) {
        // EIT
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return true; // no need to resync stream
    }

    if ((0xCC==table_id) && (VersionETT(psip.tsheader()->PID())==version)) {
        // ETT
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return true; // no need to resync stream
    }

    return false;
}

#define DECODE_VCT 1
#define DECODE_EIT 0
#define DECODE_ETT 0
#define DECODE_STT 0

void ATSCStreamData::HandleTables(const TSPacket* tspacket, HDTVRecorder* recorder)
{
#define HT_RETURN { delete psip; return; }
    PSIPTable *psip = AssemblePSIP(tspacket);
    if (!psip)
        return;
    const int version = psip->Version();
    if (!psip->IsGood()) {
        VERBOSE(VB_RECORD, QString("PSIP packet failed CRC check"));
        HT_RETURN;
    }
    if (!psip->IsCurrent()) // we don't cache the next table, for now
        HT_RETURN;


    if (1!=tspacket->AdaptationFieldControl())
    { // payload only, ATSC req.
        VERBOSE(VB_RECORD, QString("PSIP packet has Adaptation Field Control, not ATSC compiant"));
        HT_RETURN;
    }
    if (tspacket->ScramplingControl())
    { // scrambled! ATSC, DVB require tables not to be scrambled
        VERBOSE(VB_RECORD, QString("PSIP packet is scrambled, not ATSC/DVB compiant"));
        HT_RETURN;
    }

    if (IsRedundant(*psip)) {
        if (0x00==psip->TableID())
            if (recorder) recorder->WritePAT();
        if (0x02==psip->TableID())
            if (recorder) recorder->WritePMT();
        HT_RETURN; // already parsed this table, toss it.
    }

    switch (psip->TableID()) {
        case 0x00: {
            if (CreatePAT(ProgramAssociationTable(*psip)))
                if (recorder) recorder->WritePAT();
            HT_RETURN;
        }
        case 0x02: {
            CreatePMT(ProgramMapTable(*psip));
            if (recorder) recorder->WritePMT();
            HT_RETURN;
        }
        case 0xC7: {
            SetVersionMGT(version);
            MasterGuideTable mgt(*psip);

            VERBOSE(VB_RECORD, mgt.toString());

            for (unsigned int i=0; i<mgt.TableCount(); i++)
                AddListeningPID(mgt.TablePID(i));
            HT_RETURN;
        }
    }

    if (VersionMGT()<0)
        HT_RETURN;

    switch (psip->TableID()) {
        case 0xC8:
        if (DECODE_VCT) {
            VERBOSE(VB_RECORD, QString("VCT Terra"));
            TerrestrialVirtualChannelTable vct(*psip);
            SetVersionTVCT(vct.TransportStreamID(), version);

            if (vct.ChannelCount() < 1) {
                VERBOSE(VB_IMPORTANT, "TVCT: table has no channels");
                HT_RETURN;
            }

            bool found = false;

            for (uint i=0; i<vct.ChannelCount(); i++) {
                VERBOSE(VB_RECORD, vct.toString(i));
                if (vct.MinorChannel(i)==(uint)DesiredSubchannel()) {
                    VERBOSE(VB_RECORD, QString("***Desired subchannel %1")
                            .arg(DesiredSubchannel()));
                    if (vct.ProgramNumber(i) != DesiredProgram()) {
                        VERBOSE(VB_RECORD, 
                                QString("Resetting desired program from %1"
                                        " to %2").arg(DesiredProgram())
                                .arg(vct.ProgramNumber(i)));
                        // Do a (partial?) reset here if old desired
                        // program is not 0?
                        setDesiredProgram(vct.ProgramNumber(i));
                        found = true;
                    }
                }
            }

            if (!found) {
                VERBOSE(VB_IMPORTANT, 
                        QString("Desired subchannel %1 not found;"
                                " using %2 instead")
                        .arg(DesiredSubchannel()).arg(vct.MinorChannel(0)));
                setDesiredProgram(vct.ProgramNumber(0));
            }
        }
        break;
        case 0xC9: VERBOSE(VB_RECORD, QString("VCT Cable"));    break;
        case 0xCA: VERBOSE(VB_RECORD, QString("RRT"));    break;
        case 0xCB: // EIT
        if (DECODE_EIT) {
            SetVersionEIT(tspacket->PID(), version);
            EventInformationTable eit(*psip);
            VERBOSE(VB_RECORD, QString("EIT pid(%1) ").arg(tspacket->PID()).append(eit.toString()));
        }
        break;
        case 0xCC: // ETT
        if (DECODE_ETT) {
            SetVersionETT(tspacket->PID(), version);
            ExtendedTextTable ett(*psip);
            VERBOSE(VB_RECORD, QString("ETT pid(%1) ").arg(tspacket->PID()).append(ett.toString()));
        }
        break;
        case 0xCD: // STT
        if (DECODE_STT) {
            SystemTimeTable stt(*psip);
            if (stt.GPSOffset() != _GPS_UTC_offset) // only update if it changes
                _GPS_UTC_offset = stt.GPSOffset();
            VERBOSE(VB_RECORD, QString("STT pid(%1) ").arg(tspacket->PID()).append(stt.toString()));
        }
        break;
        case 0xD3: VERBOSE(VB_RECORD, QString("DCCT"));   break;
        case 0xD4: VERBOSE(VB_RECORD, QString("DCCSCT")); break;
        default:   VERBOSE(VB_RECORD, QString("unknown table 0x%1")
                           .arg(psip->TableID(),0,16)); break;
    }
    HT_RETURN;
#undef HT_RETURN
}

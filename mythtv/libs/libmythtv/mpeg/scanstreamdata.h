// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef SCANSTREAMDATA_H_
#define SCANSTREAMDATA_H_

#include "atscstreamdata.h"
#include "dvbstreamdata.h"

class ScanStreamData : virtual public MPEGStreamData,
             virtual public ATSCStreamData,
             virtual public DVBStreamData
{
    Q_OBJECT
  public:
    ScanStreamData() : QObject(NULL, "ScanStreamData"),
                       MPEGStreamData(-1, true),
                       ATSCStreamData(-1,-1, true), DVBStreamData(true) { ; }
    virtual ~ScanStreamData();

    bool IsRedundant(const PSIPTable&) const;
    bool HandleTables(uint pid, const PSIPTable &psip);

    void Reset();

  signals:
    // ATSC
    void UpdateMGT(   const MasterGuideTable*);
    void UpdateSTT(   const SystemTimeTable*);
    void UpdateRRT(   const RatingRegionTable*);
    void UpdateDCCT(  const DirectedChannelChangeTable*);
    void UpdateDCCSCT(const DirectedChannelChangeSelectionCodeTable*);

    void UpdateVCT( uint pid, const VirtualChannelTable*);
    void UpdateTVCT(uint pid, const TerrestrialVirtualChannelTable*);
    void UpdateCVCT(uint pid, const CableVirtualChannelTable*);
    void UpdateEIT( uint pid, const EventInformationTable*);
    void UpdateETT( uint pid, const ExtendedTextTable*);

    // DVB
    void UpdateNIT(const NetworkInformationTable*);
    void UpdateSDT(uint tsid, const ServiceDescriptionTable*);

  private:
};

#endif // SCANSTREAMDATA_H_

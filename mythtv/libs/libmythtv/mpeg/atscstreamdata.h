// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef ATSCSTREAMDATA_H_
#define ATSCSTREAMDATA_H_

#include "mpegstreamdata.h"

class PESPacket;
class ProgramAssociationTable;
class ProgramMapTable;
class TSPacket_nonconst;
class HDTVRecorder;
class PSIPTable;
class RingBuffer;

class ATSCStreamData : public MPEGStreamData {
  public:
    ATSCStreamData(int desiredChannel, int desiredSubchannel) :
        MPEGStreamData(desiredChannel, desiredSubchannel), _mgt_version(-1),
        _GPS_UTC_offset(13 /* leap seconds as of 2004 */) { ; }

    virtual inline void Reset(int desiredChannel = -1,
                              int desiredSubchannel = -1) {
        MPEGStreamData::Reset(desiredChannel, desiredSubchannel);
        _mgt_version = -1;
        _tvct_version.clear();
        _eit_version.clear();
        _ett_version.clear();
    }

    void SetVersionMGT(int ver) { _mgt_version = ver; }
    int VersionMGT() const { return _mgt_version; }

    void SetVersionTVCT(unsigned int pid, int version) { _tvct_version[pid]=version; }
    int VersionTVCT(unsigned int pid) const {
        const QMap<unsigned int, int>::const_iterator it = _tvct_version.find(pid);
        return (it==_tvct_version.end()) ? -1 : *it;
    }

    void SetVersionEIT(unsigned int pid, int version) { _eit_version[pid]=version; }
    int VersionEIT(unsigned int pid) const {
        const QMap<unsigned int, int>::const_iterator it = _eit_version.find(pid);
        return (it==_eit_version.end()) ? -1 : *it;
    }

    void SetVersionETT(unsigned int pid, int version) {        _ett_version[pid]=version; }
    int VersionETT(unsigned int pid) const {
        const QMap<unsigned int, int>::const_iterator it = _ett_version.find(pid);
        return (it==_ett_version.end()) ? -1 : *it;
    }

    void HandleTables(const TSPacket* tspacket, HDTVRecorder*);
    bool IsRedundant(const PSIPTable&) const;

    uint GPSOffset() { return _GPS_UTC_offset; }
  private:
    int _mgt_version;
    uint _GPS_UTC_offset;
    QMap<unsigned int, int> _tvct_version;
    QMap<unsigned int, int> _eit_version;
    QMap<unsigned int, int> _ett_version;
};

#endif

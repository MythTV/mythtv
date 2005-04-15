// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEGSTREAMDATA_H_
#define MPEGSTREAMDATA_H_

#include <qmap.h>
#include "tspacket.h"

class ProgramAssociationTable;
class ProgramMapTable;
class HDTVRecorder;
class PSIPTable;
class RingBuffer;

class MPEGStreamData {
  public:
    MPEGStreamData(int desiredChannel, int desiredSubchannel) :
        _pat(0), _pmt(0), _desired_program(-1),
        _desired_channel(desiredChannel),
        _desired_subchannel(desiredSubchannel)
    {
        _pids_listening[MPEG_PAT_PID] = true;
        _pids_listening[ATSC_PSIP_PID] = true;

        _pid_video = _pid_pmt = 0xffffffff;
    }
    virtual ~MPEGStreamData();
    virtual void Reset(int desiredChannel = -1, int desiredSubchannel = -1);

    PSIPTable* AssemblePSIP(const TSPacket* tspacket);
    bool AssemblePSIP(PSIPTable& psip, TSPacket* tspacket);

    inline void SetPAT(ProgramAssociationTable* pPAT);
    ProgramAssociationTable* PAT() { return _pat; }
    const ProgramAssociationTable* PAT() const { return _pat; }
    int VersionPAT() const;

    void SetPMT(ProgramMapTable* pPMT);
    ProgramMapTable* PMT() { return _pmt; }
    const ProgramMapTable* PMT() const { return _pmt; }
    int VersionPMT() const;

    bool CreatePAT(const ProgramAssociationTable&);
    bool CreatePMT(const ProgramMapTable&);
    unsigned int VideoPID() const { return _pid_video; }

    void AddListeningPID(unsigned int pid) { _pids_listening[pid] = true; }
    void RemoveListeningPID(unsigned int pid) { _pids_listening.erase(pid); }
    bool IsListeningPID(unsigned int pid) const {
        QMap<unsigned int, bool>::const_iterator it = _pids_listening.find(pid);
        return it != _pids_listening.end();
    }

    void AddNotListeningPID(unsigned int pid) { _pids_notlistening[pid] = true; }
    void RemoveNotListeningPID(unsigned int pid) { _pids_notlistening.erase(pid); }
    bool IsNotListeningPID(unsigned int pid) const {
        QMap<unsigned int, bool>::const_iterator it = _pids_notlistening.find(pid);
        return it != _pids_notlistening.end();
    }

    void AddWritingPID(unsigned int pid) { _pids_writing[pid] = true; }
    void RemoveWritingPID(unsigned int pid) { _pids_writing.erase(pid); }
    bool IsWritingPID(unsigned int pid) const {
        QMap<unsigned int, bool>::const_iterator it = _pids_writing.find(pid);
        return it != _pids_writing.end();
    }

    void AddAudioPID(unsigned int pid) { _pids_audio[pid] = true; }
    void RemoveAudioPID(unsigned int pid) { _pids_audio.erase(pid); }
    bool IsAudioPID(unsigned int pid) const {
        QMap<unsigned int, bool>::const_iterator it = _pids_audio.find(pid);
        return it != _pids_audio.end();
    }

    inline void SavePartialPES(unsigned int pid, PESPacket* packet);
    PESPacket* GetPartialPES(unsigned int pid) { return _partial_pes_packet_cache[pid]; }
    void ClearPartialPES(unsigned int pid) { _partial_pes_packet_cache.remove(pid); }
    void DeletePartialPES(unsigned int pid);

    inline void HandleAdaptationFieldControl(const TSPacket* tspacket);

    int DesiredProgram() const { return _desired_program; }
    int DesiredChannel() const { return _desired_channel; }
    int DesiredSubchannel() const { return _desired_subchannel; }
  protected:
    void setDesiredProgram(int p) { _desired_program = p; }
  private:
    ProgramAssociationTable* _pat;
    ProgramMapTable* _pmt;

    int _desired_program;
    int _desired_channel;
    int _desired_subchannel;
    unsigned int _pid_video;
    unsigned int _pid_pmt;

    QMap<unsigned int, bool> _pids_listening;
    QMap<unsigned int, bool> _pids_notlistening;
    QMap<unsigned int, bool> _pids_writing;
    QMap<unsigned int, bool> _pids_audio;
    QMap<unsigned long long, PESPacket*> _partial_pes_packet_cache;
};

#include "mpegtables.h"

inline void MPEGStreamData::SavePartialPES(unsigned int pid, PESPacket* packet) {
    QMap<unsigned long long, PESPacket*>::Iterator it = _partial_pes_packet_cache.find(pid);
    if (it==_partial_pes_packet_cache.end())
        _partial_pes_packet_cache[pid] = packet;
    else
    {
        PESPacket *old = *it;
        _partial_pes_packet_cache.replace(pid, packet);
        delete old;
    }
}

inline void MPEGStreamData::SetPAT(ProgramAssociationTable* pat) {
    if (_pat)
        delete _pat;
    _pat = pat;
}

inline void MPEGStreamData::SetPMT(ProgramMapTable* pmt) {
    if (_pmt)
        delete _pmt;
    _pmt = pmt;
}

inline int MPEGStreamData::VersionPAT() const { 
    return (_pat) ? int(_pat->Version()) : -1;
}

inline int MPEGStreamData::VersionPMT() const {
    return (_pmt) ? int(_pmt->Version()) : -1;
}

inline void MPEGStreamData::HandleAdaptationFieldControl(const TSPacket*)
{
    // TODO
    //AdaptationFieldControl afc(tspacket.data()+4);
}

#endif 

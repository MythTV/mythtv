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

typedef QMap<unsigned int, PESPacket*> pid_pes_map_t;

class MPEGStreamData : public QObject
{
    Q_OBJECT
  public:
    MPEGStreamData(int desiredProgram, bool cacheTables);
    virtual ~MPEGStreamData();
    virtual void Reset(int desiredProgram);

    virtual bool IsRedundant(const PSIPTable&) const;
    virtual void HandleTables(const TSPacket* tspacket);
    virtual bool ProcessTSPacket(const TSPacket& tspacket);
    virtual int  ProcessData(unsigned char *buffer, int len);

    PSIPTable* AssemblePSIP(const TSPacket* tspacket);
    bool AssemblePSIP(PSIPTable& psip, TSPacket* tspacket);

    void AddListeningPID(uint pid)       { _pids_listening[pid] = true; }
    void AddNotListeningPID(uint pid)    { _pids_notlistening[pid] = true; }
    void AddWritingPID(uint pid)         { _pids_writing[pid] = true; }
    void AddAudioPID(uint pid)           { _pids_audio[pid] = true; }

    void RemoveListeningPID(uint pid)    { _pids_listening.erase(pid); }
    void RemoveNotListeningPID(uint pid) { _pids_notlistening.erase(pid); }
    void RemoveWritingPID(uint pid)      { _pids_writing.erase(pid); }
    void RemoveAudioPID(uint pid)        { _pids_audio.erase(pid); }

    bool IsListeningPID(uint pid) const;
    bool IsNotListeningPID(uint pid) const;
    bool IsWritingPID(uint pid) const;
    bool IsAudioPID(uint pid) const;

    const QMap<uint, bool>& ListeningPIDs() const { return _pids_listening; }

    void SavePartialPES(uint pid, PESPacket* packet);
    PESPacket* GetPartialPES(uint pid)
        { return _partial_pes_packet_cache[pid]; }
    void ClearPartialPES(uint pid) { _partial_pes_packet_cache.remove(pid); }
    void DeletePartialPES(uint pid);


    void SetVersionPMT(uint pid, int version) { _pmt_version[pid] = version; }
    inline int VersionPMT(uint pid) const;

    inline void HandleAdaptationFieldControl(const TSPacket* tspacket);

  signals:
    void UpdatePAT(const ProgramAssociationTable*);
    void UpdatePMT(uint pid, const ProgramMapTable*);

  public:
    // Single program stuff, sets
    void SetDesiredProgram(int p) { _desired_program = p; }
    inline void SetPATSingleProgram(ProgramAssociationTable* pPAT);
    inline void SetPMTSingleProgram(ProgramMapTable* pPMT);

    // Single program stuff, gets
    int DesiredProgram() const { return _desired_program; }
    uint VideoPIDSingleProgram() const { return _pid_video_single_program; }

    const ProgramAssociationTable* PATSingleProgram() const
        { return _pat_single_program; }
    const ProgramMapTable* PMTSingleProgram() const
        { return _pmt_single_program; }

    ProgramAssociationTable* PATSingleProgram()
        { return _pat_single_program; }
    ProgramMapTable* PMTSingleProgram()
        { return _pmt_single_program; }

    // Single program stuff, mostly used internally
    int VersionPATSingleProgram() const;
    int VersionPMTSingleProgram() const;

    bool CreatePATSingleProgram(const ProgramAssociationTable&);
    bool CreatePMTSingleProgram(const ProgramMapTable&);

  signals:
    // Single Program Stuff, signals with processed tables
    void UpdatePATSingleProgram(ProgramAssociationTable*);
    void UpdatePMTSingleProgram(ProgramMapTable*);

  protected:
    static int ResyncStream(unsigned char *buffer, int curr_pos, int len);

    bool             _cache_tables;
    QMap<uint, bool> _pids_listening;
    QMap<uint, bool> _pids_notlistening;
    QMap<uint, bool> _pids_writing;
    QMap<uint, bool> _pids_audio;
    QMap<uint, int>  _pmt_version;
    pid_pes_map_t    _partial_pes_packet_cache;

    // Single program variables
    int                      _desired_program;
    uint                     _pid_video_single_program;
    uint                     _pid_pmt_single_program;
    ProgramAssociationTable* _pat_single_program;
    ProgramMapTable*         _pmt_single_program;
};

#include "mpegtables.h"

inline void MPEGStreamData::SetPATSingleProgram(ProgramAssociationTable* pat)
{
    if (_pat_single_program)
        delete _pat_single_program;
    _pat_single_program = pat;
}

inline void MPEGStreamData::SetPMTSingleProgram(ProgramMapTable* pmt)
{
    if (_pmt_single_program)
        delete _pmt_single_program;
    _pmt_single_program = pmt;
}

inline int MPEGStreamData::VersionPATSingleProgram() const
{ 
    return (_pat_single_program) ? int(_pat_single_program->Version()) : -1;
}

inline int MPEGStreamData::VersionPMTSingleProgram() const
{
    return (_pmt_single_program) ? int(_pmt_single_program->Version()) : -1;
}

inline void MPEGStreamData::HandleAdaptationFieldControl(const TSPacket*)
{
    // TODO
    //AdaptationFieldControl afc(tspacket.data()+4);
}

inline int MPEGStreamData::VersionPMT(uint pid) const
{
    const QMap<uint, int>::const_iterator it = _pmt_version.find(pid);
    return (it == _pmt_version.end()) ? -1 : *it;
}

#endif 

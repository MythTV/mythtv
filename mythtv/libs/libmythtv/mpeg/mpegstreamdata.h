// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEGSTREAMDATA_H_
#define MPEGSTREAMDATA_H_

#include <vector>
using namespace std;

#include <qmap.h>
#include "tspacket.h"
#include "util.h"

class ProgramAssociationTable;
class ProgramMapTable;
class HDTVRecorder;
class PSIPTable;
class RingBuffer;
class PESPacket;

typedef QMap<unsigned int, PESPacket*> pid_pes_map_t;
typedef QMap<const PSIPTable*, int>    psip_refcnt_map_t;
typedef vector<const ProgramMapTable*> pmt_vec_t;
typedef QMap<uint, const ProgramMapTable*> pmt_map_t;
typedef QMap<uint, ProgramMapTable*>   pmt_cache_t;

class MPEGStreamData : public QObject
{
    Q_OBJECT
  public:
    MPEGStreamData(int desiredProgram, bool cacheTables);
    virtual ~MPEGStreamData();

    void SetCaching(bool cacheTables) { _cache_tables = cacheTables; }
    virtual void Reset(int desiredProgram);

    // Table processing
    void SetIgnoreCRC(bool haveCRCbug) { _have_CRC_bug = haveCRCbug; }
    virtual bool IsRedundant(const PSIPTable&) const;
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    virtual void HandleTSTables(const TSPacket* tspacket);
    virtual bool ProcessTSPacket(const TSPacket& tspacket);
    virtual int  ProcessData(unsigned char *buffer, int len);
    inline  void HandleAdaptationFieldControl(const TSPacket* tspacket);

    // Listening
    virtual void AddListeningPID(uint pid)  { _pids_listening[pid] = true; }
    virtual void AddNotListeningPID(uint pid){_pids_notlistening[pid] = true;}
    virtual void AddWritingPID(uint pid)    { _pids_writing[pid] = true;   }
    virtual void AddAudioPID(uint pid)      { _pids_audio[pid] = true;     }

    virtual void RemoveListeningPID(uint pid) { _pids_listening.erase(pid);  }
    virtual void RemoveNotListeningPID(uint pid)
        { _pids_notlistening.erase(pid); }
    virtual void RemoveWritingPID(uint pid) { _pids_writing.erase(pid);    }
    virtual void RemoveAudioPID(uint pid)   { _pids_audio.erase(pid);      }

    virtual bool IsListeningPID(uint pid) const;
    virtual bool IsNotListeningPID(uint pid) const;
    virtual bool IsWritingPID(uint pid) const;
    virtual bool IsAudioPID(uint pid) const;

    virtual const QMap<uint, bool>& ListeningPIDs(void) const
        { return _pids_listening; }

    // Table versions
    virtual void SetVersionPAT(int version) { _pat_version = version;  }
    virtual int  VersionPAT(void) const     { return _pat_version;     }
    virtual void SetVersionPMT(uint program_num, int ver)
        { _pmt_version[program_num] = ver; }
    virtual inline int VersionPMT(uint program_num) const;

    // Caching
    virtual bool HasCachedPAT(void) const;
    virtual bool HasCachedPMT(uint program_num) const;
    virtual bool HasAllPMTsCached(void) const;

    virtual const ProgramAssociationTable *GetCachedPAT(void) const;
    virtual const ProgramMapTable *GetCachedPMT(uint program_num) const;
    virtual pmt_vec_t GetCachedPMTs(void) const;
    virtual pmt_map_t GetCachedPMTMap(void) const;

    virtual void ReturnCachedTable(const PSIPTable *psip) const;
    virtual void ReturnCachedTables(pmt_vec_t&) const;
    virtual void ReturnCachedTables(pmt_map_t&) const;

  signals:
    void UpdatePAT(const ProgramAssociationTable*);
    void UpdatePMT(uint program_num, const ProgramMapTable*);

  public:
    // Single program stuff, sets
    void SetDesiredProgram(int p)           { _desired_program = p;    }
    inline void SetPATSingleProgram(ProgramAssociationTable*);
    inline void SetPMTSingleProgram(ProgramMapTable*);
    void SetVideoStreamsRequired(uint num)
        { _pmt_single_program_num_video = num;  }
    uint GetVideoStreamsRequired() const
        { return _pmt_single_program_num_video; }
    void SetAudioStreamsRequired(uint num)
        { _pmt_single_program_num_audio = num;  }
    uint GetAudioStreamsRequired() const
        { return _pmt_single_program_num_audio; }

    // Single program stuff, gets
    int DesiredProgram(void) const          { return _desired_program; }
    uint VideoPIDSingleProgram(void) const  { return _pid_video_single_program; }

    const ProgramAssociationTable* PATSingleProgram(void) const
        { return _pat_single_program; }
    const ProgramMapTable* PMTSingleProgram(void) const
        { return _pmt_single_program; }

    ProgramAssociationTable* PATSingleProgram(void)
        { return _pat_single_program; }
    ProgramMapTable* PMTSingleProgram(void)
        { return _pmt_single_program; }

    // Single program stuff, mostly used internally
    int VersionPATSingleProgram(void) const;
    int VersionPMTSingleProgram(void) const;

    bool CreatePATSingleProgram(const ProgramAssociationTable&);
    bool CreatePMTSingleProgram(const ProgramMapTable&);

  signals:
    // Single Program Stuff, signals with processed tables
    void UpdatePATSingleProgram(ProgramAssociationTable*);
    void UpdatePMTSingleProgram(ProgramMapTable*);

  protected:
    // Table processing -- for internal use
    PSIPTable* AssemblePSIP(const TSPacket* tspacket, bool& moreTablePackets);
    bool AssemblePSIP(PSIPTable& psip, TSPacket* tspacket);
    void SavePartialPES(uint pid, PESPacket* packet);
    PESPacket* GetPartialPES(uint pid)
        { return _partial_pes_packet_cache[pid]; }
    void ClearPartialPES(uint pid)
        { _partial_pes_packet_cache.remove(pid); }
    void DeletePartialPES(uint pid);
    void ProcessPAT(const ProgramAssociationTable *pat);
    void ProcessPMT(const uint pid, const ProgramMapTable *pmt);

    static int ResyncStream(unsigned char *buffer, int curr_pos, int len);

    // Caching
    void IncrementRefCnt(const PSIPTable *psip) const;
    virtual void DeleteCachedTable(PSIPTable *psip) const;
    void CachePAT(ProgramAssociationTable *pat);
    void CachePMT(uint program_num, ProgramMapTable *pmt);

  protected:
    bool                      _have_CRC_bug;

    // Listening
    QMap<uint, bool>          _pids_listening;
    QMap<uint, bool>          _pids_notlistening;
    QMap<uint, bool>          _pids_writing;
    QMap<uint, bool>          _pids_audio;

    // Table versions
    int                       _pat_version;
    QMap<uint, int>           _pmt_version;

    // PSIP construction 
    pid_pes_map_t             _partial_pes_packet_cache;

    // Caching
    bool                             _cache_tables;
    mutable QMutex                   _cache_lock;
    mutable ProgramAssociationTable *_cached_pat;
    mutable pmt_cache_t              _cached_pmts;
    mutable psip_refcnt_map_t        _cached_ref_cnt;
    mutable psip_refcnt_map_t        _cached_slated_for_deletion;

    // Single program variables
    int                       _desired_program;
    uint                      _pid_video_single_program;
    uint                      _pid_pmt_single_program;
    uint                      _pmt_single_program_num_video;
    uint                      _pmt_single_program_num_audio;
    ProgramAssociationTable  *_pat_single_program;
    ProgramMapTable          *_pmt_single_program;

  // PAT Timeout handling.
  private:
    bool                      _invalid_pat_seen;
    bool                      _invalid_pat_warning;
    MythTimer                 _invalid_pat_timer;
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

inline int MPEGStreamData::VersionPMT(uint prog_num) const
{
    const QMap<uint, int>::const_iterator it = _pmt_version.find(prog_num);
    return (it == _pmt_version.end()) ? -1 : *it;
}

#endif 

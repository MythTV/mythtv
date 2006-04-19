// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEGSTREAMDATA_H_
#define MPEGSTREAMDATA_H_

#include <vector>
using namespace std;

#include <qmap.h>
#include "tspacket.h"
#include "util.h"
#include "streamlisteners.h"

class PSIPTable;
class RingBuffer;
class PESPacket;


typedef QMap<unsigned int, PESPacket*>  pid_pes_map_t;
typedef QMap<const PSIPTable*, int>     psip_refcnt_map_t;

typedef ProgramAssociationTable*        pat_ptr_t;

typedef ProgramMapTable*                pmt_ptr_t;
typedef vector<const ProgramMapTable*>  pmt_vec_t;
typedef QMap<uint, pmt_vec_t>           pmt_map_t;
typedef QMap<uint, ProgramMapTable*>    pmt_cache_t;

typedef vector<unsigned char>           uchar_vec_t;
typedef uchar_vec_t                     sections_t;
typedef QMap<uint, sections_t>          sections_map_t;

typedef vector<MPEGStreamListener*>     mpeg_listener_vec_t;
typedef vector<MPEGSingleProgramStreamListener*> mpeg_sp_listener_vec_t;

void init_sections(sections_t &sect, uint last_section);

class MPEGStreamData
{
  public:
    MPEGStreamData(int desiredProgram, bool cacheTables);
    virtual ~MPEGStreamData();

    void SetCaching(bool cacheTables) { _cache_tables = cacheTables; }
    virtual void Reset(int desiredProgram);

    // Table processing
    void SetIgnoreCRC(bool haveCRCbug) { _have_CRC_bug = haveCRCbug; }
    virtual bool IsRedundant(uint pid, const PSIPTable&) const;
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

    virtual QMap<uint, bool> ListeningPIDs(void) const
        { return _pids_listening; }

    // Table versions
    void SetVersionPAT(int version, uint last_section)
    {
        if (_pat_version == version)
            return;
        _pat_version = version;
        init_sections(_pat_section_seen, last_section);
    }
    int  VersionPAT(void) const { return _pat_version; }

    void SetVersionPMT(uint program_num, int version, uint last_section)
    {
        if (VersionPMT(program_num) == version)
            return;
        _pmt_version[program_num] = version;
        init_sections(_pmt_section_seen[program_num], last_section);
    }
    int  VersionPMT(uint prog_num) const
    {
        const QMap<uint, int>::const_iterator it = _pmt_version.find(prog_num);
        if (it == _pmt_version.end())
            return -1;
        return *it;
    }

    // Sections seen
    void SetPATSectionSeen(uint section);
    bool PATSectionSeen(uint section) const;
    bool HasAllPATSections(void) const;

    void SetPMTSectionSeen(uint prog_num, uint section);
    bool PMTSectionSeen(uint prog_num, uint section) const;
    bool HasAllPMTSections(uint prog_num) const;

    // Caching
    bool HasCachedPAT(void) const;
    bool HasCachedAllPMT(uint program_num) const;
    bool HasCachedAnyPMT(uint program_num) const;
    bool HasCachedAllPMTs(void) const;

    const pat_ptr_t GetCachedPAT(void) const;
    const pmt_ptr_t GetCachedPMT(uint program_num, uint section_num) const;
    pmt_vec_t GetCachedPMTs(void) const;
    pmt_map_t GetCachedPMTMap(void) const;

    virtual void ReturnCachedTable(const PSIPTable *psip) const;
    virtual void ReturnCachedTables(pmt_vec_t&) const;
    virtual void ReturnCachedTables(pmt_map_t&) const;

    // "signals"
    void AddMPEGListener(MPEGStreamListener*);
    void RemoveMPEGListener(MPEGStreamListener*);
    void UpdatePAT(const ProgramAssociationTable*);
    void UpdateCAT(const ConditionalAccessTable*);
    void UpdatePMT(uint program_num, const ProgramMapTable*);

    // Single Program Stuff, signals with processed tables
    void AddMPEGSPListener(MPEGSingleProgramStreamListener*);
    void RemoveMPEGSPListener(MPEGSingleProgramStreamListener*);
    void UpdatePATSingleProgram(ProgramAssociationTable*);
    void UpdatePMTSingleProgram(ProgramMapTable*);

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
    void CachePAT(const ProgramAssociationTable *pat);
    void CachePMT(const ProgramMapTable *pmt);

  protected:
    bool                      _have_CRC_bug;

    // Listening
    QMap<uint, bool>          _pids_listening;
    QMap<uint, bool>          _pids_notlistening;
    QMap<uint, bool>          _pids_writing;
    QMap<uint, bool>          _pids_audio;

    // Signals
    mutable QMutex            _listener_lock;
    mpeg_listener_vec_t       _mpeg_listeners;
    mpeg_sp_listener_vec_t    _mpeg_sp_listeners;

    // Table versions
    int                       _pat_version;
    QMap<uint, int>           _pmt_version;

    sections_t                _pat_section_seen;
    sections_map_t            _pmt_section_seen;

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

  protected:
    static const unsigned char bit_sel[8];
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

#endif 

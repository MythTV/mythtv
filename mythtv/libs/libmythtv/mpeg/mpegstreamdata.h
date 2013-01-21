// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEGSTREAMDATA_H_
#define MPEGSTREAMDATA_H_

// POSIX
#include <stdint.h>  // uint64_t

// C++
#include <vector>
using namespace std;

// Qt
#include <QMap>

#include "tspacket.h"
#include "mythtimer.h"
#include "streamlisteners.h"
#include "eitscanner.h"
#include "mythtvexp.h"

class EITHelper;
class PSIPTable;
class RingBuffer;

typedef vector<uint>                    uint_vec_t;

typedef QMap<unsigned int, PSIPTable*>  pid_psip_map_t;
typedef QMap<const PSIPTable*, int>     psip_refcnt_map_t;

typedef ProgramAssociationTable*               pat_ptr_t;
typedef ProgramAssociationTable const*         pat_const_ptr_t;
typedef vector<const ProgramAssociationTable*> pat_vec_t;
typedef QMap<uint, pat_vec_t>                  pat_map_t;
typedef QMap<uint, ProgramAssociationTable*>   pat_cache_t;

typedef ConditionalAccessTable*                cat_ptr_t;
typedef ConditionalAccessTable const*          cat_const_ptr_t;
typedef vector<const ConditionalAccessTable*>  cat_vec_t;
typedef QMap<uint, cat_vec_t>                  cat_map_t;
typedef QMap<uint, ConditionalAccessTable*>    cat_cache_t;

typedef ProgramMapTable*                pmt_ptr_t;
typedef ProgramMapTable const*          pmt_const_ptr_t;
typedef vector<const ProgramMapTable*>  pmt_vec_t;
typedef QMap<uint, pmt_vec_t>           pmt_map_t;
typedef QMap<uint, ProgramMapTable*>    pmt_cache_t;

typedef vector<unsigned char>           uchar_vec_t;
typedef uchar_vec_t                     sections_t;
typedef QMap<uint, sections_t>          sections_map_t;

typedef vector<MPEGStreamListener*>     mpeg_listener_vec_t;
typedef vector<TSPacketListener*>       ts_listener_vec_t;
typedef vector<TSPacketListenerAV*>     ts_av_listener_vec_t;
typedef vector<MPEGSingleProgramStreamListener*> mpeg_sp_listener_vec_t;

typedef enum
{
    kEncUnknown   = 0,
    kEncDecrypted = 1,
    kEncEncrypted = 2,
} CryptStatus;

class MTV_PUBLIC CryptInfo
{
  public:
    CryptInfo() :
        status(kEncUnknown), encrypted_packets(0), decrypted_packets(0),
        encrypted_min(1000), decrypted_min(8) { }
    CryptInfo(uint e, uint d) :
        status(kEncUnknown), encrypted_packets(0), decrypted_packets(0),
        encrypted_min(e), decrypted_min(d) { }

  public:
    CryptStatus status;
    uint encrypted_packets;
    uint decrypted_packets;
    uint encrypted_min;
    uint decrypted_min;
};

void init_sections(sections_t &sect, uint last_section);

typedef enum
{
    kPIDPriorityNone   = 0,
    kPIDPriorityLow    = 1,
    kPIDPriorityNormal = 2,
    kPIDPriorityHigh   = 3,
} PIDPriority;
typedef QMap<uint, PIDPriority> pid_map_t;

class MTV_PUBLIC MPEGStreamData : public EITSource
{
  public:
    MPEGStreamData(int desiredProgram, bool cacheTables);
    virtual ~MPEGStreamData();

    void SetCaching(bool cacheTables) { _cache_tables = cacheTables; }
    void SetListeningDisabled(bool lt) { _listening_disabled = lt; }

    virtual void Reset(void) { MPEGStreamData::ResetMPEG(-1); }
    virtual void ResetMPEG(int desiredProgram);

    /// \brief Current Offset from computer time to DVB time in seconds
    double TimeOffset(void) const;

    // EIT Source
    virtual void SetEITHelper(EITHelper *eit_helper);
    virtual void SetEITRate(float rate);
    virtual bool HasEITPIDChanges(const uint_vec_t& /*in_use_pids*/) const
        { return false; }
    virtual bool GetEITPIDChanges(const uint_vec_t& /*in_use_pids*/,
                                  uint_vec_t& /*add_pids*/,
                                  uint_vec_t& /*del_pids*/) const
        { return false; }

    // Table processing
    void SetIgnoreCRC(bool haveCRCbug) { _have_CRC_bug = haveCRCbug; }
    virtual bool IsRedundant(uint pid, const PSIPTable&) const;
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    virtual void HandleTSTables(const TSPacket* tspacket);
    virtual bool ProcessTSPacket(const TSPacket& tspacket);
    virtual int  ProcessData(const unsigned char *buffer, int len);
    inline  void HandleAdaptationFieldControl(const TSPacket* tspacket);

    // Listening
    virtual void AddListeningPID(
        uint pid, PIDPriority priority = kPIDPriorityNormal)
        { _pids_listening[pid] = priority; }
    virtual void AddNotListeningPID(uint pid)
        { _pids_notlistening[pid] = kPIDPriorityNormal; }
    virtual void AddWritingPID(
        uint pid, PIDPriority priority = kPIDPriorityHigh)
        { _pids_writing[pid] = priority; }
    virtual void AddAudioPID(
        uint pid, PIDPriority priority = kPIDPriorityHigh)
        { _pids_audio[pid] = priority; }

    virtual void RemoveListeningPID(uint pid) { _pids_listening.remove(pid);  }
    virtual void RemoveNotListeningPID(uint pid)
        { _pids_notlistening.remove(pid); }
    virtual void RemoveWritingPID(uint pid) { _pids_writing.remove(pid);    }
    virtual void RemoveAudioPID(uint pid)   { _pids_audio.remove(pid);      }

    virtual bool IsListeningPID(uint pid) const;
    virtual bool IsNotListeningPID(uint pid) const;
    virtual bool IsWritingPID(uint pid) const;
    bool IsVideoPID(uint pid) const
        { return _pid_video_single_program == pid; }
    virtual bool IsAudioPID(uint pid) const;

    const pid_map_t& ListeningPIDs(void) const
        { return _pids_listening; }
    const pid_map_t& AudioPIDs(void) const
        { return _pids_audio; }
    const pid_map_t& WritingPIDs(void) const
        { return _pids_writing; }

    uint GetPIDs(pid_map_t&) const;

    // PID Priorities
    PIDPriority GetPIDPriority(uint pid) const;

    // Table versions
    void SetVersionPAT(uint tsid, int version, uint last_section)
    {
        if (VersionPAT(tsid) == version)
            return;
        _pat_version[tsid] = version;
        init_sections(_pat_section_seen[tsid], last_section);
    }
    int  VersionPAT(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _pat_version.find(tsid);
        if (it == _pat_version.end())
            return -1;
        return *it;
    }

    void SetVersionCAT(uint tsid, int version, uint last_section)
    {
        if (VersionCAT(tsid) == version)
            return;
        _cat_version[tsid] = version;
        init_sections(_cat_section_seen[tsid], last_section);
    }
    int  VersionCAT(uint tsid) const
    {
        const QMap<uint, int>::const_iterator it = _cat_version.find(tsid);
        if (it == _cat_version.end())
            return -1;
        return *it;
    }

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
    void SetPATSectionSeen(uint tsid, uint section);
    bool PATSectionSeen(   uint tsid, uint section) const;
    bool HasAllPATSections(uint tsid) const;

    void SetCATSectionSeen(uint tsid, uint section);
    bool CATSectionSeen(   uint tsid, uint section) const;
    bool HasAllCATSections(uint tsid) const;

    void SetPMTSectionSeen(uint prog_num, uint section);
    bool PMTSectionSeen(   uint prog_num, uint section) const;
    bool HasAllPMTSections(uint prog_num) const;

    // Caching
    bool HasProgram(uint progNum) const;

    bool HasCachedAllPAT(uint tsid) const;
    bool HasCachedAnyPAT(uint tsid) const;
    bool HasCachedAnyPAT(void) const;

    bool HasCachedAllCAT(uint tsid) const;
    bool HasCachedAnyCAT(uint tsid) const;
    bool HasCachedAnyCAT(void) const;

    bool HasCachedAllPMT(uint program_num) const;
    bool HasCachedAnyPMT(uint program_num) const;
    bool HasCachedAllPMTs(void) const;
    bool HasCachedAnyPMTs(void) const;

    pat_const_ptr_t GetCachedPAT(uint tsid, uint section_num) const;
    pat_vec_t GetCachedPATs(uint tsid) const;
    pat_vec_t GetCachedPATs(void) const;
    pat_map_t GetCachedPATMap(void) const;

    cat_const_ptr_t GetCachedCAT(uint tsid, uint section_num) const;
    cat_vec_t GetCachedCATs(uint tsid) const;
    cat_vec_t GetCachedCATs(void) const;
    cat_map_t GetCachedCATMap(void) const;

    pmt_const_ptr_t GetCachedPMT(uint program_num, uint section_num) const;
    pmt_vec_t GetCachedPMTs(void) const;
    pmt_map_t GetCachedPMTMap(void) const;

    virtual void ReturnCachedTable(const PSIPTable *psip) const;
    virtual void ReturnCachedPATTables(pat_vec_t&) const;
    virtual void ReturnCachedPATTables(pat_map_t&) const;
    virtual void ReturnCachedCATTables(cat_vec_t&) const;
    virtual void ReturnCachedCATTables(cat_map_t&) const;
    virtual void ReturnCachedPMTTables(pmt_vec_t&) const;
    virtual void ReturnCachedPMTTables(pmt_map_t&) const;

    // Encryption Monitoring
    void AddEncryptionTestPID(uint pnum, uint pid, bool isvideo);
    void RemoveEncryptionTestPIDs(uint pnum);
    bool IsEncryptionTestPID(uint pid) const;

    void TestDecryption(const ProgramMapTable* pmt);
    void ResetDecryptionMonitoringState(void);

    bool IsProgramDecrypted(uint pnum) const;
    bool IsProgramEncrypted(uint pnum) const;

    // "signals"
    void AddMPEGListener(MPEGStreamListener*);
    void RemoveMPEGListener(MPEGStreamListener*);
    void UpdatePAT(const ProgramAssociationTable*);
    void UpdateCAT(const ConditionalAccessTable*);
    void UpdatePMT(uint program_num, const ProgramMapTable*);

    void AddWritingListener(TSPacketListener*);
    void RemoveWritingListener(TSPacketListener*);

    // Single Program Stuff, signals with processed tables
    void AddMPEGSPListener(MPEGSingleProgramStreamListener*);
    void RemoveMPEGSPListener(MPEGSingleProgramStreamListener*);
    void AddAVListener(TSPacketListenerAV*);
    void RemoveAVListener(TSPacketListenerAV*);
    void UpdatePATSingleProgram(ProgramAssociationTable*);
    void UpdatePMTSingleProgram(ProgramMapTable*);

  public:
    // Single program stuff, sets
    void SetDesiredProgram(int p);
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
    void SetRecordingType(const QString &recording_type);

    // Single program stuff, gets
    int DesiredProgram(void) const          { return _desired_program; }
    uint VideoPIDSingleProgram(void) const  { return _pid_video_single_program; }
    QString GetRecordingType(void) const;

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
    void SavePartialPSIP(uint pid, PSIPTable* packet);
    PSIPTable* GetPartialPSIP(uint pid)
        { return _partial_psip_packet_cache[pid]; }
    void ClearPartialPSIP(uint pid)
        { _partial_psip_packet_cache.remove(pid); }
    void DeletePartialPSIP(uint pid);
    void ProcessPAT(const ProgramAssociationTable *pat);
    void ProcessCAT(const ConditionalAccessTable *cat);
    void ProcessPMT(const ProgramMapTable *pmt);
    void ProcessEncryptedPacket(const TSPacket&);

    static int ResyncStream(const unsigned char *buffer, int curr_pos, int len);

    void UpdateTimeOffset(uint64_t si_utc_time);

    // Caching
    void IncrementRefCnt(const PSIPTable *psip) const;
    virtual bool DeleteCachedTable(PSIPTable *psip) const;
    void CachePAT(const ProgramAssociationTable *pat);
    void CacheCAT(const ConditionalAccessTable *pat);
    void CachePMT(const ProgramMapTable *pmt);

  protected:
    QString                   _sistandard;

    bool                      _have_CRC_bug;

    mutable QMutex            _si_time_lock;
    uint                      _si_time_offset_cnt;
    uint                      _si_time_offset_indx;
    double                    _si_time_offsets[16];

    // Generic EIT stuff used for ATSC and DVB
    EITHelper                *_eit_helper;
    float                     _eit_rate;

    // Listening
    pid_map_t                 _pids_listening;
    pid_map_t                 _pids_notlistening;
    pid_map_t                 _pids_writing;
    pid_map_t                 _pids_audio;
    bool                      _listening_disabled;

    // Encryption monitoring
    mutable QMutex            _encryption_lock;
    QMap<uint, CryptInfo>     _encryption_pid_to_info;
    QMap<uint, uint_vec_t>    _encryption_pnum_to_pids;
    QMap<uint, uint_vec_t>    _encryption_pid_to_pnums;
    QMap<uint, CryptStatus>   _encryption_pnum_to_status;

    // Signals
    mutable QMutex            _listener_lock;
    mpeg_listener_vec_t       _mpeg_listeners;
    mpeg_sp_listener_vec_t    _mpeg_sp_listeners;
    ts_listener_vec_t         _ts_writing_listeners;
    ts_av_listener_vec_t      _ts_av_listeners;

    // Table versions
    QMap<uint, int>           _pat_version;
    QMap<uint, int>           _cat_version;
    QMap<uint, int>           _pmt_version;

    sections_map_t            _pat_section_seen;
    sections_map_t            _cat_section_seen;
    sections_map_t            _pmt_section_seen;

    // PSIP construction
    pid_psip_map_t            _partial_psip_packet_cache;

    // Caching
    bool                             _cache_tables;
    mutable QMutex                   _cache_lock;
    mutable pat_cache_t              _cached_pats;
    mutable cat_cache_t              _cached_cats;
    mutable pmt_cache_t              _cached_pmts;
    mutable psip_refcnt_map_t        _cached_ref_cnt;
    mutable psip_refcnt_map_t        _cached_slated_for_deletion;

    // Single program variables
    int                       _desired_program;
    QString                   _recording_type;
    bool                      _strip_pmt_descriptors;
    bool                      _normalize_stream_type;
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

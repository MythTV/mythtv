// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEGSTREAMDATA_H_
#define MPEGSTREAMDATA_H_

// C++
#include <cstdint>  // uint64_t
#include <vector>

// Qt
#include <QMap>

#include "libmythbase/mythtimer.h"
#include "libmythtv/eitscanner.h"
#include "libmythtv/mythtvexp.h"

#include "streamlisteners.h"
#include "tablestatus.h"
#include "tspacket.h"

class EITHelper;
class PSIPTable;

using uint_vec_t = std::vector<uint>;

using pid_psip_map_t    = QMap<unsigned int, PSIPTable*>;
using psip_refcnt_map_t = QMap<const PSIPTable*, int>;

using pat_ptr_t         = ProgramAssociationTable *;
using pat_const_ptr_t   = const ProgramAssociationTable *;
using pat_vec_t         = std::vector<const ProgramAssociationTable *>;
using pat_map_t         = QMap<uint, pat_vec_t>;
using pat_cache_t       = QMap<uint, ProgramAssociationTable*>;

using cat_ptr_t         = ConditionalAccessTable *;
using cat_const_ptr_t   = const ConditionalAccessTable *;
using cat_vec_t         = std::vector<const ConditionalAccessTable *>;
using cat_map_t         = QMap<uint, cat_vec_t>;
using cat_cache_t       = QMap<uint, ConditionalAccessTable*>;

using pmt_ptr_t         = ProgramMapTable*;
using pmt_const_ptr_t   = ProgramMapTable const*;
using pmt_vec_t         = std::vector<const ProgramMapTable*>;
using pmt_map_t         = QMap<uint, pmt_vec_t>;
using pmt_cache_t       = QMap<uint, ProgramMapTable*>;

using uchar_vec_t       = std::vector<unsigned char>;

using mpeg_listener_vec_t    = std::vector<MPEGStreamListener*>;
using ts_listener_vec_t      = std::vector<TSPacketListener*>;
using ts_av_listener_vec_t   = std::vector<TSPacketListenerAV*>;
using mpeg_sp_listener_vec_t = std::vector<MPEGSingleProgramStreamListener*>;
using ps_listener_vec_t      = std::vector<PSStreamListener*>;

enum CryptStatus : std::uint8_t
{
    kEncUnknown   = 0,
    kEncDecrypted = 1,
    kEncEncrypted = 2,
};

class MTV_PUBLIC CryptInfo
{
  public:
    CryptInfo() = default;
    CryptInfo(uint e, uint d) : m_encryptedMin(e), m_decryptedMin(d) { }

  public:
    CryptStatus m_status            {kEncUnknown};
    uint        m_encryptedPackets {0};
    uint        m_decryptedPackets {0};
    uint        m_encryptedMin     {1000};
    uint        m_decryptedMin     {8};
};

enum PIDPriority : std::uint8_t
{
    kPIDPriorityNone   = 0,
    kPIDPriorityLow    = 1,
    kPIDPriorityNormal = 2,
    kPIDPriorityHigh   = 3,
};
using pid_map_t = QMap<uint, PIDPriority>;

class MTV_PUBLIC MPEGStreamData : public EITSource
{
  public:
    MPEGStreamData(int desiredProgram, int cardnum, bool cacheTables);
    ~MPEGStreamData() override;

    void SetCaching(bool cacheTables) { m_cacheTables = cacheTables; }
    void SetListeningDisabled(bool lt) { m_listeningDisabled = lt; }

    virtual void Reset(void) { Reset(-1); }
    virtual void Reset(int desiredProgram);

    /// \brief Current Offset from computer time to DVB time in seconds
    double TimeOffset(void) const;

    // EIT Source
    void SetEITHelper(EITHelper *eit_helper) override; // EITSource
    void SetEITRate(float rate) override; // EITSource
    virtual bool HasEITPIDChanges(const uint_vec_t& /*in_use_pids*/) const
        { return false; }
    virtual bool GetEITPIDChanges(const uint_vec_t& /*in_use_pids*/,
                                  uint_vec_t& /*add_pids*/,
                                  uint_vec_t& /*del_pids*/) const
        { return false; }

    // Table processing
    void SetIgnoreCRC(bool haveCRCbug) { m_haveCrcBug = haveCRCbug; }
    virtual bool IsRedundant(uint pid, const PSIPTable &psip) const;
    virtual bool HandleTables(uint pid, const PSIPTable &psip);
    virtual void HandleTSTables(const TSPacket* tspacket);
    virtual bool ProcessTSPacket(const TSPacket& tspacket);
    virtual int  ProcessData(const unsigned char *buffer, int len);
    inline  void HandleAdaptationFieldControl(const TSPacket* tspacket);

    // Listening
    virtual void AddListeningPID(
        uint pid, PIDPriority priority = kPIDPriorityNormal)
        { m_pidsListening[pid] = priority; }
    virtual void AddNotListeningPID(uint pid)
        { m_pidsNotListening[pid] = kPIDPriorityNormal; }
    virtual void AddWritingPID(
        uint pid, PIDPriority priority = kPIDPriorityHigh)
        { m_pidsWriting[pid] = priority; }
    virtual void AddAudioPID(
        uint pid, PIDPriority priority = kPIDPriorityHigh)
        { m_pidsAudio[pid] = priority; }
    virtual void AddConditionalAccessPID(
        uint pid, PIDPriority priority = kPIDPriorityNormal)
        { m_pidsConditionalAccess[pid] = priority; }

    virtual void RemoveListeningPID(uint pid) { m_pidsListening.remove(pid);  }
    virtual void RemoveNotListeningPID(uint pid)
        { m_pidsNotListening.remove(pid); }
    virtual void RemoveWritingPID(uint pid) { m_pidsWriting.remove(pid);    }
    virtual void RemoveAudioPID(uint pid)   { m_pidsAudio.remove(pid);      }

    virtual bool IsListeningPID(uint pid) const;
    virtual bool IsNotListeningPID(uint pid) const;
    virtual bool IsWritingPID(uint pid) const;
    bool IsVideoPID(uint pid) const
        { return m_pidVideoSingleProgram == pid; }
    virtual bool IsAudioPID(uint pid) const;
    virtual bool IsConditionalAccessPID(uint pid) const;

    const pid_map_t& ListeningPIDs(void) const
        { return m_pidsListening; }
    const pid_map_t& AudioPIDs(void) const
        { return m_pidsAudio; }
    const pid_map_t& WritingPIDs(void) const
        { return m_pidsWriting; }

    uint GetPIDs(pid_map_t &pids) const;

    // PID Priorities
    PIDPriority GetPIDPriority(uint pid) const;

    // Table versions
    void SetVersionPAT(uint tsid, int version, uint last_section)
    {
        m_patStatus.SetVersion(tsid, version, last_section);
    }
    void SetVersionPMT(uint pnum, int version, uint last_section)
    {
        m_pmtStatus.SetVersion(pnum, version, last_section);
    }

    // Sections seen
    bool HasAllPATSections(uint tsid) const;

    bool HasAllCATSections(uint tsid) const;

    bool HasAllPMTSections(uint prog_num) const;

    // Caching
    bool HasProgram(uint progNum) const;

    bool HasCachedAllPAT(uint tsid) const;
    bool HasCachedAnyPAT(uint tsid) const;
    bool HasCachedAnyPAT(void) const;

    bool HasCachedAllCAT(uint tsid) const;
    bool HasCachedAnyCAT(uint tsid) const;
    bool HasCachedAnyCAT(void) const;

    bool HasCachedAllPMT(uint pnum) const;
    bool HasCachedAnyPMT(uint pnum) const;
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
    virtual void ReturnCachedPATTables(pat_vec_t &pats) const;
    virtual void ReturnCachedPATTables(pat_map_t &pats) const;
    virtual void ReturnCachedCATTables(cat_vec_t &cats) const;
    virtual void ReturnCachedCATTables(cat_map_t &cats) const;
    virtual void ReturnCachedPMTTables(pmt_vec_t &pmts) const;
    virtual void ReturnCachedPMTTables(pmt_map_t &pmts) const;

    // Encryption Monitoring
    void AddEncryptionTestPID(uint pnum, uint pid, bool isvideo);
    void RemoveEncryptionTestPIDs(uint pnum);
    bool IsEncryptionTestPID(uint pid) const;

    void TestDecryption(const ProgramMapTable* pmt);
    void ResetDecryptionMonitoringState(void);

    bool IsProgramDecrypted(uint pnum) const;
    bool IsProgramEncrypted(uint pnum) const;

    // "signals"
    void AddMPEGListener(MPEGStreamListener *val);
    void RemoveMPEGListener(MPEGStreamListener *val);

    void AddWritingListener(TSPacketListener *val);
    void RemoveWritingListener(TSPacketListener *val);

    // Single Program Stuff, signals with processed tables
    void AddMPEGSPListener(MPEGSingleProgramStreamListener *val);
    void RemoveMPEGSPListener(MPEGSingleProgramStreamListener *val);
    void AddAVListener(TSPacketListenerAV *val);
    void RemoveAVListener(TSPacketListenerAV *val);

    // Program Stream Stuff
    void AddPSStreamListener(PSStreamListener *val);
    void RemovePSStreamListener(PSStreamListener *val);

  public:
    // Single program stuff, sets
    void SetDesiredProgram(int p);
    inline void SetPATSingleProgram(ProgramAssociationTable *pat);
    inline void SetPMTSingleProgram(ProgramMapTable *pmt);
    void SetVideoStreamsRequired(uint num)
        { m_pmtSingleProgramNumVideo = num;  }
    uint GetVideoStreamsRequired() const
        { return m_pmtSingleProgramNumVideo; }
    void SetAudioStreamsRequired(uint num)
        { m_pmtSingleProgramNumAudio = num;  }
    uint GetAudioStreamsRequired() const
        { return m_pmtSingleProgramNumAudio; }
    void SetRecordingType(const QString &recording_type);

    // Single program stuff, gets
    int DesiredProgram(void) const          { return m_desiredProgram; }
    uint VideoPIDSingleProgram(void) const  { return m_pidVideoSingleProgram; }
    QString GetRecordingType(void) const    { return m_recordingType; }

    const ProgramAssociationTable* PATSingleProgram(void) const
        { return m_patSingleProgram; }
    const ProgramMapTable* PMTSingleProgram(void) const
        { return m_pmtSingleProgram; }

    ProgramAssociationTable* PATSingleProgram(void)
        { return m_patSingleProgram; }
    ProgramMapTable* PMTSingleProgram(void)
        { return m_pmtSingleProgram; }

    // Single program stuff, mostly used internally
    int VersionPATSingleProgram(void) const;
    int VersionPMTSingleProgram(void) const;

    bool CreatePATSingleProgram(const ProgramAssociationTable &pat);
    bool CreatePMTSingleProgram(const ProgramMapTable &pmt);

  protected:
    // Table processing -- for internal use
    PSIPTable* AssemblePSIP(const TSPacket* tspacket, bool& moreTablePackets);
    bool AssemblePSIP(PSIPTable& psip, TSPacket* tspacket);
    void SavePartialPSIP(uint pid, PSIPTable* packet);
    PSIPTable* GetPartialPSIP(uint pid)
        { return m_partialPsipPacketCache[pid]; }
    void ClearPartialPSIP(uint pid)
        { m_partialPsipPacketCache.remove(pid); }
    void DeletePartialPSIP(uint pid);
    void ProcessPAT(const ProgramAssociationTable *pat);
    void ProcessCAT(const ConditionalAccessTable *cat);
    void ProcessPMT(const ProgramMapTable *pmt);
    void ProcessEncryptedPacket(const TSPacket &tspacket);

    static int ResyncStream(const unsigned char *buffer, int curr_pos, int len);

    void UpdateTimeOffset(uint64_t si_utc_time);

    // Caching
    void IncrementRefCnt(const PSIPTable *psip) const;
    virtual bool DeleteCachedTable(const PSIPTable *psip) const;
    void CachePAT(const ProgramAssociationTable *pat);
    void CacheCAT(const ConditionalAccessTable *_cat);
    void CachePMT(const ProgramMapTable *pmt);

  protected:
    int                       m_cardId;
    QString                   m_siStandard                  {"mpeg"};

    bool                      m_haveCrcBug                  {false};

    mutable QMutex            m_siTimeLock;
    uint                      m_siTimeOffsetCnt             {0};
    uint                      m_siTimeOffsetIndx            {0};
    std::array<double,16>     m_siTimeOffsets               {0.0};

    // Generic EIT stuff used for ATSC and DVB
    EITHelper                *m_eitHelper                   {nullptr};
    float                     m_eitRate                     {1.0F};

    // Listening
    pid_map_t                 m_pidsListening;
    pid_map_t                 m_pidsNotListening;
    pid_map_t                 m_pidsWriting;
    pid_map_t                 m_pidsAudio;
    pid_map_t                 m_pidsConditionalAccess;
    bool                      m_listeningDisabled           {false};

    // Encryption monitoring
    mutable QRecursiveMutex   m_encryptionLock;
    QMap<uint, CryptInfo>     m_encryptionPidToInfo;
    QMap<uint, uint_vec_t>    m_encryptionPnumToPids;
    QMap<uint, uint_vec_t>    m_encryptionPidToPnums;
    QMap<uint, CryptStatus>   m_encryptionPnumToStatus;

    // Signals
    mutable QRecursiveMutex   m_listenerLock;
    mpeg_listener_vec_t       m_mpegListeners;
    mpeg_sp_listener_vec_t    m_mpegSpListeners;
    ts_listener_vec_t         m_tsWritingListeners;
    ts_av_listener_vec_t      m_tsAvListeners;
    ps_listener_vec_t         m_psListeners;

    // Table versions
    TableStatusMap            m_patStatus;
    TableStatusMap            m_catStatus;
    TableStatusMap            m_pmtStatus;

    // PSIP construction
    pid_psip_map_t            m_partialPsipPacketCache;

    // Caching
    bool                             m_cacheTables;
    mutable QRecursiveMutex          m_cacheLock;
    mutable pat_cache_t              m_cachedPats;
    mutable cat_cache_t              m_cachedCats;
    mutable pmt_cache_t              m_cachedPmts;
    mutable psip_refcnt_map_t        m_cachedRefCnt;
    mutable psip_refcnt_map_t        m_cachedSlatedForDeletion;

    // Single program variables
    int                       m_desiredProgram;
    QString                   m_recordingType               {"all"};
    bool                      m_stripPmtDescriptors         {false};
    bool                      m_normalizeStreamType         {true};
    uint                      m_pidVideoSingleProgram       {0xffffffff};
    uint                      m_pidPmtSingleProgram         {0xffffffff};
    uint                      m_pmtSingleProgramNumVideo    {1};
    uint                      m_pmtSingleProgramNumAudio    {0};
    ProgramAssociationTable  *m_patSingleProgram            {nullptr};
    ProgramMapTable          *m_pmtSingleProgram            {nullptr};

  // PAT Timeout handling.
  private:
    bool                      m_invalidPatSeen              {false};
    bool                      m_invalidPatWarning           {false};
    MythTimer                 m_invalidPatTimer;
};

#include "mpegtables.h"

inline void MPEGStreamData::SetPATSingleProgram(ProgramAssociationTable* pat)
{
    delete m_patSingleProgram;
    m_patSingleProgram = pat;
}

inline void MPEGStreamData::SetPMTSingleProgram(ProgramMapTable* pmt)
{
    delete m_pmtSingleProgram;
    m_pmtSingleProgram = pmt;
}

inline int MPEGStreamData::VersionPATSingleProgram() const
{
    return (m_patSingleProgram) ? int(m_patSingleProgram->Version()) : -1;
}

inline int MPEGStreamData::VersionPMTSingleProgram() const
{
    return (m_pmtSingleProgram) ? int(m_pmtSingleProgram->Version()) : -1;
}

inline void MPEGStreamData::HandleAdaptationFieldControl(const TSPacket */*tspacket*/)
{
    // TODO
    //AdaptationFieldControl afc(tspacket.data()+4);
}

#endif

/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 */

#ifndef SIPARSER_H
#define SIPARSER_H

// C includes
#include <cstdio>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <unistd.h>
#include <sys/types.h>

// Qt includes
#include <qstringlist.h>
#include <qobject.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include <qtextcodec.h>
#include <qmutex.h>

// MythTV includes
#include "eitscanner.h"
#include "sitypes.h"

class EITHelper;
class DVBRecorder;

class ProgramAssociationTable;
class ConditionalAccessTable;
class ProgramMapTable;

class ATSCStreamData;
class DVBStreamData;
class MPEGStreamData;

class MasterGuideTable;
class VirtualChannelTable;
class SystemTimeTable;
class EventInformationTable;
class ExtendedTextTable;

class NetworkInformationTable;
class ServiceDescriptionTable;
class DVBEventInformationTable;

class SIParser : public QObject, public EITSource
{
    Q_OBJECT
  public:
    SIParser(const char *name = "SIParser");
    ~SIParser();

    int Start(void);

    void SetATSCStreamData(ATSCStreamData*);
    void SetDVBStreamData(DVBStreamData*);
    void SetStreamData(MPEGStreamData*);

    void SetTableStandard(const QString&);
    void SetDesiredProgram(uint mpeg_program_number, bool reset = true);
    void ReinitSIParser(const QString &si_std,
                        MPEGStreamData *stream_data,
                        uint mpeg_program_number);

    // Stops all collection of data and clears all values
    // (on a channel change for example)
    void Reset(void);

    virtual void AddPid(uint, uint8_t, uint8_t, bool, uint) = 0;
    virtual void DelPid(uint) = 0;
    virtual void DelAllPids(void) = 0;

    void ParseTable(uint8_t* buffer, int size, uint16_t pid);

    void SetDishNetEIT(bool on)
        { eit_dn_long = on; }

    void SetEITHelper(EITHelper *helper)
        { QMutexLocker locker(&pmap_lock); eit_helper = helper; }
    void SetEITRate(float rate)
        { QMutexLocker locker(&pmap_lock); eit_rate = rate; }

  public slots:
    virtual void deleteLater(void);

    // MPEG
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*);
    void HandlePMT(uint pnum, const ProgramMapTable*);

    // ATSC
    void HandleMGT(const MasterGuideTable*);
    void HandleSTT(const SystemTimeTable*);
    void HandleVCT(uint pid, const VirtualChannelTable*);
    void HandleEIT(uint pid, const EventInformationTable*);
    void HandleETT(uint pid, const ExtendedTextTable*);

    // DVB
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleEIT(const DVBEventInformationTable*);

  signals:
    void UpdatePMT(uint pid, const ProgramMapTable *pmt);

  protected:
    void CheckTrackers(void);
    void AdjustEITPids(void);

  private:
    // DVB Descriptor Parsers
    void HandleNITDesc(const desc_list_t &dlist);
    void HandleNITTransportDesc(const desc_list_t &dlist,
                                TransportObject   &tobj,
                                QMap_uint16_t     &clist);

  private:
    // Common Variables
    DVBRecorder        *dvb_recorder;
    int                 table_standard;

    // Storage Objects (DVB)
    NITObject           NITList;

    // Storage Objects (ATSC)
    QMap<uint,uint>     sourceid_to_channel;
    ATSCStreamData     *atsc_stream_data;
    DVBStreamData      *dvb_stream_data;

    // Mutex Locks
    // TODO: Lock Events, and Services, Transports, etc
    QMutex              pmap_lock;
    pnum_pid_map_t      pnum_pid;
    dvb_srv_eit_on_t    dvb_srv_collect_eit;
    atsc_eit_pid_map_t  atsc_eit_pid;
    atsc_ett_pid_map_t  atsc_ett_pid;
    atsc_eit_pid_map_t  atsc_eit_inuse_pid;
    atsc_ett_pid_map_t  atsc_ett_inuse_pid;

    int                 ThreadRunning;
    bool                exitParserThread;

    // New tracking objects
    TableHandler       *Table[NumHandlers+1];

    bool                eit_reset;
    /// Rate at which EIT data is collected
    float               eit_rate;
    /// Decode DishNet's long-term DVB EIT
    bool                eit_dn_long;
    /// Sink for eit events
    EITHelper          *eit_helper;
};

#endif // SIPARSER_H


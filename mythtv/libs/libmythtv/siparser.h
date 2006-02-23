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

// C++ includes
#include <iostream>
using namespace std;

// Qt includes
#include <qstringlist.h>
#include <qobject.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qmap.h>
#include <qtextcodec.h>
#include <qmutex.h>

// MythTV includes
#include "sitypes.h"

#ifdef USING_DVB_EIT
#include "eitfixup.h"
#else // if !USING_DVB_EIT
typedef void QMap_Events;
#endif // !USING_DVB_EIT

class ProgramAssociationTable;
class ProgramMapTable;
class MasterGuideTable;
class VirtualChannelTable;
class SystemTimeTable;
class EventInformationTable;
class ExtendedTextTable;
class NetworkInformationTable;
class ServiceDescriptionTable;
class DVBEventInformationTable;

/**
 *  Custom descriptors allow or disallow HUFFMAN_TEXT - For North American 
 *  DVB providers who use Huffman compressed guide in the 9x descriptors.
 */
#define CUSTOM_DESC_HUFFMAN_TEXT               1

/**
 *  Custom descriptors allow or disallow CHANNEL_NUMBERS - For the UK where
 *  channel numbers are sent in one of the SDT tables (at least per scan.c docs)
 */
#define CUSTOM_DESC_CHANNEL_NUMBERS         2

/**
 * The guide source pid.
 */
#define GUIDE_STANDARD                0

/**
 *  GUIDE_DATA_PID is for nonstandard PID being used for EIT style guide
 *  this is seen in North America (this only applies to DVB)
 */
#define GUIDE_DATA_PID                1

/**
 *  Post processing of the guide.  Some carriers put all of the event text
 *  into the description (subtitle, acotors, etc).  You can post processes these
 *  types of carriers EIT data using some simple RegExps to get more powerful
 *  guide data.  BellExpressVu in Canada is one example.
 */
#define GUIDE_POST_PROCESS_EXTENDED        1

class SIParser : public QObject
{
    Q_OBJECT
  public:
    SIParser(const char *name = "SIParser");
    ~SIParser();

    int Start(void);

    // Generic functions that will begin collection of tables based on the
    // SIStandard.
    bool FindTransports(void);
    bool FindServices(void);
    bool FillPMap(SISTANDARD _SIStandard);
    bool FillPMap(const QString&);

    bool AddPMT(uint16_t ServiceID);

    bool ReinitSIParser(const QString &si_std, uint service_id);

    // Stops all collection of data and clears all values (on a channel change for example)
    void Reset(void);

    virtual void AddPid(uint16_t pid,uint8_t mask,uint8_t filter,bool CheckCRC,int bufferFactor);
    virtual void DelPid(int pid);
    virtual void DelAllPids(void);

    // Functions that may become signals for communication with the outside world
    void ServicesComplete(void);
    void GuideComplete(void);
//    void EventsReady(void);

    // Functions to get objects into other classes for manipulation
    bool GetTransportObject(NITObject &NIT);
    bool GetServiceObject(QMap_SDTObject &SDT);

    void ParseTable(uint8_t* buffer, int size, uint16_t pid);
    void CheckTrackers(void);

  public slots:
    virtual void deleteLater(void);

    void HandlePAT(const ProgramAssociationTable*);
    void HandlePMT(uint pnum, const ProgramMapTable*);
    void HandleMGT(const MasterGuideTable*);
    void HandleSTT(const SystemTimeTable*);
    void HandleVCT(uint pid, const VirtualChannelTable*);
    void HandleEIT(uint pid, const EventInformationTable*);
    void HandleETT(uint pid, const ExtendedTextTable*);
    void HandleNIT(const NetworkInformationTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleEIT(const DVBEventInformationTable*);

  signals:
    void FindTransportsComplete(void);
    void FindServicesComplete(void);
    void FindEventsComplete(void);
    void TableLoaded(void);
    void UpdatePMT(const PMTObject *pmt);
    void EventsReady(QMap_Events* Events);
    void AllEventsPulled(void);

  protected:
    void PrintDescriptorStatistics(void) const;

  private:
    uint GetLanguagePriority(const QString &language);

    // Fixes for various DVB Network Spec Deviations
    void LoadDVBSpecDeviations(uint16_t NetworkID);

    void LoadPrivateTypes(uint networkID);

    // MPEG Transport Parsers (ATSC and DVB)
    tablehead_t       ParseTableHead(uint8_t* buffer, int size);

    void ParseCAT(uint pid, tablehead_t* head, uint8_t* buffer, uint size);

    void ProcessUnusedDescriptor(uint pid, const uint8_t *buffer, uint size);

    // DVB Helper Parsers
    QDateTime ConvertDVBDate(const uint8_t* dvb_buf);

    // Common Descriptor Parsers
    CAPMTObject ParseDescCA(const uint8_t* buffer, int size);

    // DVB Descriptor Parsers
    void HandleNITDesc(const desc_list_t &dlist);
    void HandleNITTransportDesc(const desc_list_t &dlist,
                                TransportObject   &tobj,
                                QMap_uint16_t     &clist);
    void ParseDescService      (const uint8_t* buf, int sz,
                                SDTObject       &s);
    void ParseDescFrequencyList(const uint8_t* buf, int sz,
                                TransportObject &t);
    void ParseDescUKChannelList(const uint8_t* buf, int sz,
                                QMap_uint16_t   &num);
    TransportObject ParseDescTerrestrial     (const uint8_t* buf, int sz);
    TransportObject ParseDescSatellite       (const uint8_t* buf, int sz);
    TransportObject ParseDescCable           (const uint8_t* buf, int sz);
    void ParseDescTeletext                   (const uint8_t* buf, int sz);
    void ParseDescSubtitling                 (const uint8_t* buf, int sz);

#ifdef USING_DVB_EIT
    // DVB EIT Table Descriptor processors
    uint ProcessDVBEventDescriptors(
        uint                         pid,
        const unsigned char          *data,
        uint                         &bestPrioritySE,
        const unsigned char*         &bestDescriptorSE, 
        uint                         &bestPriorityEE,
        vector<const unsigned char*> &bestDescriptorsEE,
        Event                        &event);

#endif //USING_DVB_EIT

  private:
    // Timeout Variables
    QDateTime TransportSearchEndTime;
    QDateTime ServiceSearchEndTime;
    QDateTime EventSearchEndTime;

    // Common Variables
    int                 SIStandard;
    uint                CurrentTransport;
    uint                RequestedServiceID;
    uint                RequestedTransportID;
    
    // Preferred languages and their priority
    QMap<QString,uint>  LanguagePriority;

    // DVB Variables
    uint                NITPID;

    // Storage Objects (DVB)
    QValueList<CAPMTObject> CATList;
    NITObject           NITList;

    // Storage Objects (ATSC)
    QMap<uint,uint>     sourceid_to_channel;

#ifdef USING_DVB_EIT
    // Storage Objects (ATSC & DVB)
    QMap2D_Events       EventMapObject;
    QMap_Events         EventList;
#endif //USING_DVB_EIT

    // Mutex Locks
    // TODO: Lock Events, and Services, Transports, etc
    QMutex              pmap_lock;

    int                 ThreadRunning;
    bool                exitParserThread;
    TableSourcePIDObject TableSourcePIDs;
    bool                ParserInReset;
    bool                standardChange;

    // New tracking objects
    TableHandler       *Table[NumHandlers+1];
    privateTypes        PrivateTypes;
    bool                PrivateTypesLoaded;

#ifdef USING_DVB_EIT
    /// EITFixUp instance
    EITFixUp            eitfixup;
#endif

    // statistics
    QMap<uint,uint>     descCount;
    mutable QMutex      descLock;
};

#endif // SIPARSER_H


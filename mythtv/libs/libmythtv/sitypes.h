/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef SITYPES_H
#define SITYPES_H

#include <qmap.h>
#include <qstringlist.h>
#include <qdatetime.h>

#include "config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/* This file will contain all of the common objects for DVB SI and 
   ATSC PSIP parsing */

typedef enum
{
    SI_STANDARD_ATSC = 0,
    SI_STANDARD_DVB,
    SI_STANDARD_AUTO
} SISTANDARD;

typedef enum ES_Type
{
    ES_TYPE_UNKNOWN,
    ES_TYPE_VIDEO_MPEG1,
    ES_TYPE_VIDEO_MPEG2,
    ES_TYPE_VIDEO_MPEG4,
    ES_TYPE_VIDEO_H264,
    ES_TYPE_AUDIO_MPEG1,
    ES_TYPE_AUDIO_MPEG2,
    ES_TYPE_AUDIO_AC3,
    ES_TYPE_AUDIO_DTS,
    ES_TYPE_AUDIO_AAC,
    ES_TYPE_TELETEXT,
    ES_TYPE_SUBTITLE,
    ES_TYPE_DATA
};

typedef enum tabletypes
{
    PAT,                /* Program Association Table */
    PMT,                /* Program Managemenet Table */
    MGT,                /* ATSC Management Table */
    STT,                /* ATSC Time Table */
    EVENTS,             /* EIT for DVB or ATSC */
    SERVICES,           /* SDT or T/CVCT */
    NETWORK,            /* Current Network NIT */

    OTHER_SERVICES,     /* Other Network SDT */
    OTHER_NETWORK,      /* Other Network NIT */
    CAT                 /* Conditional Access Table */
};

class SectionTracker;
class Event;
class pidHandler;
class SDTObject;

typedef QMap<uint16_t,SDTObject>           QMap_SDTObject;
typedef QMap<uint16_t,QMap_SDTObject>      QMap2D_SDTObject;
typedef QMap<uint16_t,uint16_t>            QMap_uint16_t;
typedef QMap<uint16_t,uint32_t>            QMap_uint32_t;
typedef QMap<uint16_t,bool>                QMap_bool;
typedef QMap<uint16_t,QString>             QMap_QString;
typedef QMap<uint16_t,QMap_QString>        QMap2D_QString;
typedef QMap<uint16_t,SectionTracker>      QMap_SectionTracker;
typedef QMap<uint16_t,QMap_SectionTracker> QMap2D_SectionTracker;
typedef QMap<uint16_t,Event>               QMap_Events;
typedef QMap<uint16_t,QMap_Events>         QMap2D_Events;
typedef QMap<uint16_t,pidHandler>          QMap_pidHandler;

class pidHandler
{
public:
    pidHandler() { reset(); }
    void reset();

    uint16_t pid;
    uint8_t mask;
    uint8_t filter;
    bool pulling;
};

/* Class for holding private types that are Network Specific and populated
   from the dtv_privatetypes table.  This is for DVB and ATSC */
class privateTypes
{
public:
    privateTypes() { reset(); };
    ~privateTypes() {};
    void reset();
    uint16_t      CurrentTransportID;

    /* Services that send SDT 0x42 as other transport */
    bool          SDTMapping;
    /* Use of general custom Descriptors */
    QStringList   Descriptors;
    /* Non Standard TV Service Types */
    QMap_uint16_t TVServiceTypes;
    /* Use one of a collection of EIT Post Parsing Scripts to improve EIT */
    int           EITFixUp;
    /* DVB Channel numbers used in UK and Australia AKA LCN */
    uint8_t       ChannelNumbers;
    /* Use Custom Guide Ranges other than 0x50-0x5F & 0x60-0x6F in DVB */
    bool          CustomGuideRanges;
    uint8_t       CurrentTransportTableMin;
    uint8_t       CurrentTransportTableMax;
    uint8_t       OtherTransportTableMin;
    uint8_t       OtherTransportTableMax;
    /* Custom Guide Parameters */
    bool          ForceGuidePresent;
    bool          CustomGuidePID;
    uint16_t      GuidePID;
    bool          GuideOnSingleTransport;
    uint16_t      GuideTransportID;
    /* List of ServiceID:s for which to parse out subtitle from the description.
       Used in EITFixUpStyle4() */
    QMap_uint16_t ParseSubtitleServiceIDs;

};

typedef struct tablehead
{
    uint8_t    table_id;
    uint16_t   section_length;
    uint16_t   table_id_ext;
    bool       current_next;
    uint8_t    version;
    uint8_t    section_number;
    uint8_t    section_last;
} tablehead_t;

/* Used for keeping track of tableloads */
class pullStatus
{
public:
    pullStatus() { Reset(); }
    ~pullStatus() {}
    void Reset();

    bool pulling;         /* Table is currently filtered */
    bool requested;       /* Table is requested to be pulled */
    bool requestedEmit;   /* Table is requested AND needs to be emited when complete */
    bool emitted;         /* Table has been emitted */
};

typedef QMap<uint16_t,pullStatus> QMap_pullStatus;

class TableHandler
{
public:
    TableHandler() {}
    virtual ~TableHandler() {}

    /* Resets the handler to empty state */
    virtual void Reset() = 0;

    /* PID Opening / Requirement functions */
    virtual bool RequirePIDs() = 0;
    virtual bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask) = 0;

    /* Request this table be pulled */
    virtual void Request(uint16_t key = 0) = 0;

    /* States table is loaded, to provide dependencies to other tables */
    virtual bool Complete() = 0;

    virtual bool AddSection(tablehead_t* head, uint16_t key0, uint16_t key1) =0;
    virtual void AddKey(uint16_t /*key0*/, uint16_t /*key1*/) {}

    /* Sets the SI Standard */
    virtual void SetSIStandard(SISTANDARD _SIStandard) { SIStandard = _SIStandard; }

    /* Adds required PIDs to table, change size to a type */
    virtual void AddPid(uint16_t pid,uint8_t filter, uint8_t mask, uint8_t key = 0)
    { (void) pid; (void) mask; (void) filter; (void) key; }

    /* Emit Functions for sending data off to siscan or dvbchannel */
    virtual void RequestEmit(uint16_t /*key*/) {}

    virtual bool EmitRequired() { return false; }
    virtual bool CompleteEmitRequired() { return false; }

    virtual void SetupTrackers() {}

    virtual bool GetEmitID(uint16_t& key0, uint16_t& key1)
    {
        (void)key0;
        (void)key1;
        return false;
    }

    /* Dependency setting funtions */
    virtual void DependencyMet(tabletypes t) = 0;
    virtual void DependencyChanged(tabletypes /*t*/) {}

    SISTANDARD SIStandard;

};

/* Object best to be removed and not used anymore */
class TableSourcePIDObject
{
public:
    TableSourcePIDObject() { Reset(); }
    void Reset();

    uint16_t TransportPID;
    uint16_t ServicesPID;
    uint8_t  ServicesTable;
    uint8_t  ServicesMask;

    // ATSC Specific
    uint16_t ChannelETT;

    // ToDo: EIT Table Stuff
    // Do array for EIT Events
};

// Classes for tracking table loads
class SectionTracker
{
public:
    SectionTracker() { Reset(); }
    /* Clear values */
    void Reset();
    /* Mark certain sections unused (Only used in DVBEIT) */
    void MarkUnused(int Section);
    /* Check for complete section load */
    int Complete();
    // Debug function to know what sections have been loaded
    QString loadStatus();

    int AddSection(tablehead *head);

private:
    int16_t MaxSections;       // Max number of sections
    int8_t  Version;           // Table Version
    uint8_t Filled[0x100];     // Fill status of each section
};

// Tables to hold Actual Network Objects
class SDTObject
{
public:
    enum {TV=1,RADIO=2};

    SDTObject() { Reset(); }
    void Reset();

    uint16_t ServiceID;
    uint16_t TransportID;
    uint8_t  EITPresent;
    uint16_t NetworkID;
    uint8_t  RunningStatus;
    uint8_t  CAStatus;
    QString  ServiceName;
    QString  ProviderName;
    int      ChanNum;
    uint8_t  ServiceType;
    uint8_t  Version;
    uint16_t ATSCSourceID;
    int      MplexID;        /* Set if MplexID is know i.e.current service*/
};

class CAPMTObject
{
public:
    CAPMTObject() { Reset(); }

    void Reset();
    uint16_t CASystemID;
    uint16_t PID;
    uint8_t  Data_Length;
    uint8_t  Data[256];
};
typedef QValueList<CAPMTObject> CAList;

class Descriptor
{
public:
    Descriptor();
    Descriptor(const uint8_t *Data, const uint8_t Length);
    Descriptor(const Descriptor &orig);
    ~Descriptor();

    uint8_t  Length;
    uint8_t  *Data;
};
typedef QValueList<Descriptor> DescriptorList;

class ElementaryPIDObject
{
public:
    ElementaryPIDObject() { Reset(); }
    void Reset();

    ES_Type Type;
    uint8_t Orig_Type;
    uint16_t PID;
    QString Description;
    QString Language;
    CAList CA;
    DescriptorList Descriptors;
    bool Record;
};
typedef QValueList<ElementaryPIDObject> ComponentList;

class Person
{
public:
    Person() {};
    Person(const QString &r, const QString &n)
        :role(r),name(n) {};
    QString role;
    QString name;
};

class Event
{
//TODO: Int conversion
public:
    Event() { Reset(); }

    void Reset();
    void clearEventValues();

    uint16_t SourcePID;    /* Used in ATSC for Checking for ETT PID being filtered */
    QDateTime StartTime;
    QDateTime EndTime;
    uint16_t TransportID;
    uint16_t NetworkID;
    QString LanguageCode;
    uint16_t ServiceID;    /* NOT the Virtual Channel Number used by ATSC */
    QString Event_Name;
    QString Event_Subtitle;
    QString Description;
    uint16_t EventID;
    QString ContentDescription;
    QString Year;
    QStringList Actors;
    bool Stereo;
    bool HDTV;
    bool SubTitled;
    int ETM_Location;    /* Used to flag still waiting ETTs for ATSC */
    bool ATSC;
    //bool PreviouslyShown;
    QDate OriginalAirDate;
    QValueList<Person> Credits;
};

// DVB TransportObject - Used with NIT Scanning
class TransportObject
{
public:
    TransportObject() { Reset(); }
    TransportObject(const QString& DVBType)
                    : Type(DVBType)
                        { }

    void Reset();

    // Many of these object types are QStrings since it will
    // go into the database easier, and also be handled by
    // ParseQPSK,ParseQAM,ParseXXX since that is what they expect
    QString Type;
    uint16_t NetworkID;
    uint16_t TransportID;

    // DVB Global
    uint32_t Frequency;
    QString Modulation;
    uint32_t SymbolRate;
    QString FEC_Inner;

    // DVB-S Specific
    QString Polarity;
    QString OrbitalLocation;

    // DVB-C Specific
    QString FEC_Outer;

    // DVB-T Speficic
    QString Bandwidth;
    QString Constellation;
    // QString CentreFrequency - Place in Frequency
    QString Hiearchy;
    QString CodeRateHP;
    QString CodeRateLP;
    QString GuardInterval;
    QString TransmissionMode;
    QString Inversion;

    //Additional frequencies
    QValueList<unsigned> frequencies;
};

class NetworkObject
{
public:
    NetworkObject() { Reset(); }
    void Reset();

    uint16_t NetworkID;
    QString NetworkName;
    uint16_t LinkageTransportID;
    uint16_t LinkageNetworkID;
    uint16_t LinkageServiceID;
    uint16_t LinkageType;
    uint8_t LinkagePresent;
};


class NITObject
{
public:
    QValueList<NetworkObject> Network;
    QValueList<TransportObject> Transport;
};

class PMTObject
{
  public:
    PMTObject() { Reset(); }
    PMTObject(const PMTObject &other);
    PMTObject& operator=(const PMTObject &other);
    void shallowCopy(const PMTObject &other);
    void deepCopy(const PMTObject &other);
    void Reset();

    /// \brief Returns true if PMT contains at least one audio stream
    bool HasAudioService() const { return hasAudio; }
    /// \brief Returns true if PMT contains at least one video stream
    bool HasVideoService() const { return hasVideo; }
    /// \brief Returns true if PMT contains at least one video stream
    ///        and one audio stream
    bool HasTelevisionService() const { return hasVideo && hasAudio; }
    /// \brief Returns true if the streams are not encrypted ("Free-To-Air")
    bool FTA() const { return !hasCA; }

    /// @deprecated Use HasTelevisionService() instead.
    bool OnAir() const { return HasTelevisionService(); }

  public:
    uint16_t       PCRPID;
    uint16_t       ServiceID;
    uint16_t       PMTPID;
    CAList         CA;
    DescriptorList Descriptors;
    ComponentList  Components;

    // helper variables
    bool           hasCA;
    bool           hasAudio;
    bool           hasVideo;
};

/* PAT Handler */
class PATHandler : public TableHandler
{
public:
    PATHandler() : TableHandler() {}
    ~PATHandler() {}

    void Reset();
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key = 0);
    bool Complete();
    void AddKey(uint16_t key0, uint16_t key1) { (void) key0; (void) key1; }
    bool AddSection(tablehead_t *head, uint16_t key0 = 0, uint16_t key1 = 0);
    void DependencyMet(tabletypes t) { (void) t; }
    void DependencyChanged(tabletypes t) { (void) t; }

    QMap_uint16_t pids;

    SectionTracker Tracker;
    pullStatus     status;
};

/* PMT Handler */
typedef QMap<uint16_t,PMTObject> QMap_PMTObject;

class PMTHandler : public TableHandler
{
public:
    PMTHandler() : TableHandler() { Reset(); }
    ~PMTHandler() {}

    void Reset() { Tracker.clear();  pmt.clear();  status.clear(); patloaded = false; }
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key = 0) { (void) key; }
    void RequestEmit(uint16_t key);
    bool EmitRequired();
    bool CompleteEmitRequired() { return false; }
    bool GetEmitID(uint16_t& key0, uint16_t& key1);
    bool AddSection(tablehead_t* head, uint16_t key0, uint16_t key1);
    bool Complete() { return false; }
    void AddKey(uint16_t key0, uint16_t key1);
    void DependencyMet(tabletypes t);
    void DependencyChanged(tabletypes t);

    QMap_PMTObject      pmt;

private:
    QMap_SectionTracker Tracker;
    bool                patloaded;
    QMap_pullStatus     status;
};

class MGTHandler : public TableHandler
{
public:
    MGTHandler() : TableHandler() { }
    ~MGTHandler() {}

    void Reset() { status.Reset();  Tracker.Reset();  pids.clear(); }
    bool RequirePIDs();
    /* It's best to open the PID wide open so you get the other ATSC tables */
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key = 0);
    bool Complete();
    void AddKey(uint16_t key0, uint16_t key1) { (void) key0; (void) key1; }
    bool AddSection(tablehead_t *head, uint16_t key0 = 0, uint16_t key1 = 0);

    void DependencyMet(tabletypes t) { (void) t; }
    void DependencyChanged(tabletypes t) { (void) t; }

    QMap_uint16_t pids;

    SectionTracker Tracker;
    pullStatus     status;
};

class STTHandler : public TableHandler
{
public:
    STTHandler() : TableHandler() {}
    ~STTHandler() {}

    void Reset() { status.Reset();  Tracker.Reset();  GPSOffset = 0; }
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key = 0);
    bool Complete();
    void AddKey(uint16_t key0, uint16_t key1) { (void) key0; (void) key1; }
    bool AddSection(tablehead_t *head, uint16_t key0 = 0, uint16_t key1 = 0);

    void DependencyMet(tabletypes t) { (void) t; }
    void DependencyChanged(tabletypes t) { (void) t; }

    int           GPSOffset;

    SectionTracker Tracker;
    pullStatus     status;
};

// TODO: Setup only for ATSC Guide right now
class EventHandler : public TableHandler
{
public:
    EventHandler() : TableHandler() { Reset(); }
    ~EventHandler() {}
    void Reset();
    void SetSIStandard(SISTANDARD _SIStandard) { SIStandard = _SIStandard; }
    bool Complete();
    void SetupTrackers();
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key) { (void) key; }
    void RequestEmit(uint16_t key);
    bool EmitRequired();
    bool GetEmitID(uint16_t& key0, uint16_t& key1);
    void DependencyMet(tabletypes t);
    void AddPid(uint16_t pid,uint8_t filter, uint8_t mask, uint8_t key);
    bool AddSection(tablehead_t *head, uint16_t key0, uint16_t key1);

    QMap2D_SectionTracker  Tracker;
    QMap_pullStatus        status;        /* Status of serviceIDs */
    bool                   mgtloaded;
    bool                   sttloaded;
    bool                   servicesloaded;
    bool                   CompleteSent;
    QMap_pidHandler        EITpid;        /* for ATSC use this as a key */
    QMap_pidHandler        ETTpid;
    QMap2D_Events          Events;
    QMap_bool              TrackerSetup;
};

class ServiceHandler : public TableHandler
{
public:
    ServiceHandler() : TableHandler() { Reset(); }
    ~ServiceHandler() {}
    void SetSIStandard(SISTANDARD _SIStandard) { SIStandard = _SIStandard; }
    void Reset();
    bool Complete();
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key);
    void RequestEmit(uint16_t key);
    bool EmitRequired() { return false; }
    bool GetEmitID(uint16_t& key0, uint16_t& key1);
    void DependencyMet(tabletypes t);
    bool AddSection(tablehead_t *head, uint16_t key0, uint16_t key1);

    QMap_SectionTracker    Tracker;
    QMap_pullStatus        status;        /* Status of each transport */
    bool                   mgtloaded;
    bool                   nitloaded;
    bool                   CompleteSent;
    QMap2D_SDTObject       Services;
};

class NetworkHandler : public TableHandler
{
public:
    NetworkHandler() : TableHandler() { Reset(); }
    ~NetworkHandler() {}
    void SetSIStandard(SISTANDARD _SIStandard) { SIStandard = _SIStandard; }
    void Reset();
    bool Complete();
    bool RequirePIDs();
    bool GetPIDs(uint16_t& pid, uint8_t& filter, uint8_t& mask);
    void Request(uint16_t key);
    void RequestEmit(uint16_t key);
    bool EmitRequired();
    bool GetEmitID(uint16_t& key0, uint16_t& key1);
    bool AddSection(tablehead_t *head, uint16_t key0, uint16_t key1);
    void DependencyMet(tabletypes t) { (void) t;}

    SISTANDARD           SIStandard;
    SectionTracker         Tracker;
    pullStatus             status;
    bool                 mgtloaded;
    bool                   CompleteSent;
    NITObject              NITList;
};

#endif // SITYPES_H

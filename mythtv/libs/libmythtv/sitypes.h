/*
 * $Id$
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef SITYPES_H
#define SITYPES_H

#include <vector>
using namespace std;

#include <qmap.h>
#include <qstringlist.h>
#include <qdatetime.h>

#include "config.h"
#include "mpegdescriptors.h"
#include "mpegtables.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef USING_DVB_EIT
#include "eit.h"
#endif

/* This file will contain all of the common objects for DVB SI and 
   ATSC PSIP parsing */

typedef enum
{
    SI_STANDARD_ATSC = 0,
    SI_STANDARD_DVB,
    SI_STANDARD_AUTO
} SISTANDARD;

static inline QString SIStandard_to_String(int std)
{
    if (SI_STANDARD_ATSC == std)
        return "atsc";
    if (SI_STANDARD_DVB == std)
        return "dvb";
    return "auto";
}


typedef enum tabletypes
{
    PAT,                /* Program Association Table */
    PMT,                /* Program Managemenet Table */
    SERVICES,           /* SDT or T/CVCT */
    NETWORK,            /* Current Network NIT */
#ifdef USING_DVB_EIT
    EVENTS,             /* EIT for DVB or ATSC */
#endif // USING_DVB_EIT

    NumHandlers,        /* placeholder */

    OTHER_SERVICES = NumHandlers, /* Other Network SDT */
    OTHER_NETWORK,      /* Other Network NIT */
    CAT,                /* Conditional Access Table */
    MGT,                /* ATSC Management Table */
    STT,                /* ATSC SystemTimeTable */
};
static QString tabletypes2string[] =
{
    "PAT",
    "PMT",
    "SERVICES",
    "NETWORK",
#ifdef USING_DVB_EIT
    "EVENTS",
#endif // USING_DVB_EIT

    "OTHER_SERVICES",
    "OTHER_NETWORK",
    "CAT",
    "MGT",
    "STT",
};


class SectionTracker;
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

/// Class for tracking DVB sections
class SectionTracker
{
  public:
    SectionTracker() : maxSections(-1), version(-1), empty(true) {;}

    /// Clear values to defaults
    void Reset(void);
    /// Mark certain tables as unused (used for EIT).
    void MarkUnused(int Section);
    /// Add a section to this tracker
    int  AddSection(const tablehead *head);

    /// Do we have all the entries?
    bool IsComplete(void) const;
    /// Do we have any entries?
    bool IsEmpty(void)    const { return empty; }
    /// Debug function to know what sections have been loaded
    QString Status(void)  const;

    typedef enum fill_status
    {
        kStatusEmpty = 0, kStatusFilled = 1,  kStatusUnused = 2
    } FillStatus;
  private:
    int        maxSections;         ///< Maximum number of sections
    int        version;             ///< PSIP Table Version
    bool       empty;               ///< do we have any section?
    vector<FillStatus> fillStatus;  ///< Fill status of each section
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

class ElementaryPIDObject
{
public:
    ElementaryPIDObject() { Reset(); }
    ~ElementaryPIDObject() { Reset(); }
    ElementaryPIDObject(const ElementaryPIDObject &other);
    ElementaryPIDObject& operator=(const ElementaryPIDObject &other);
    void deepCopy(const ElementaryPIDObject&);
    void Reset();

    QString GetLanguage(void) const
    {
        const unsigned char *d;
        d = MPEGDescriptor::Find(Descriptors, DescriptorID::ISO_639_language);
        if (!d)
            return QString::null;

        ISO639LanguageDescriptor iso_lang(d);
        return iso_lang.CanonicalLanguageString();
    }

    QString GetDescription(void) const
    {
        uint type = StreamID::Normalize(Orig_Type, Descriptors);
        QString desc = StreamID::toString(type);
        QString lang = GetLanguage();
        if (!lang.isEmpty())
            desc += QString(" (%1)").arg(lang);
        return desc;
    }

    uint        Orig_Type;
    uint        PID;
    CAList      CA;
    desc_list_t Descriptors;
    bool        Record;
};
typedef QValueList<ElementaryPIDObject> ComponentList;

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
    ~PMTObject() { Reset(); }

    PMTObject& operator=(const PMTObject &other);
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

  public:
    uint16_t       PCRPID;
    uint16_t       ServiceID;
    uint16_t       PMTPID;
    CAList         CA;
    desc_list_t    Descriptors;
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

#ifdef USING_DVB_EIT
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
#endif //USING_DVB_EIT

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

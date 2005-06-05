
#ifndef SIPARSER_H
#define SIPARSER_H

#include <stdint.h>
#include <qstringlist.h>
#include <qobject.h>
#include <qstring.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <qdatetime.h>
#include <qmap.h>
using namespace std;
#include <iostream>
//#include "dvbchannel.h"
#include "sitypes.h"
#include "qtextcodec.h"

/* TODO: Fix this size */
#define NumHandlers     7


/*
 *  Custom descriptors allow or disallow
 *  HUFFMAN_TEXT - For North American DVB providers who use Huffman compressed
 *      guide in the 9x descriptors
 *  CHANNEL_NUMBERS - For the UK where channel numbers are sent in one of the
 *      SDT tables (at least per scan.c docs)
 */
#define CUSTOM_DESC_HUFFMAN_TEXT               1
#define CUSTOM_DESC_CHANNEL_NUMBERS         2

/*
 *  Standard for parser to use.  ATSC over DVB is the same for now, but if it changes
 *  this define will need to be changed as well.
 */

/*
 * The guide source pid.
 * GUIDE_DATA_PID is for nonstandard PID being used for EIT style guide this is seen
 *    in North America (this only applies to DVB)
 */
#define GUIDE_STANDARD                0
#define GUIDE_DATA_PID                1

/*
 *  Post processing of the guide.  Some carriers put all of the event text
 *  into the description (subtitle, acotors, etc).  You can post processes these
 *  types of carriers EIT data using some simple RegExps to get more powerful
 *  guide data.  BellExpressVu in Canada is one example.
 */
#define GUIDE_POST_PROCESS_EXTENDED        1

// TODO: Remove this bcd2int conversion if possible to clean up the date
// functions since this is used by the dvbdatetime function
#define bcdtoint(i) ((((i & 0xf0) >> 4) * 10) + (i & 0x0f))

/*
 *  Copyright 2004 - Taylor Jacob (rtjacob at earthlink.net)
 *
 *  This class parses DVB SI and ATSC PSIP tables.
 *
 *  This class is generalized so it can be used with DVB Cards with a simple
 *  sct filter, sending the read data into this class, and the PCHDTV card by
 *  filtering the TS packets through another class to convert it into tables, and
 *  passing this data into this class as well.
 *
 *  Both DVB and ATSC are combined into this class since ATSC over DVB is present
 *  in some place.  (One example is PBS on AMC3 in North America).  Argentenia is
 *  also has announced their Digital TV Standard will be ATSC over DVB-T
 *
 *  Impliemntation of OpenCable or other MPEG-TS based standards (DirecTV?)
 *  is also possible with this class if their specs are ever known.
 *
 */

class SIParser : public QObject
{
    Q_OBJECT
public:

    SIParser();
    ~SIParser();

    int Start();

    // Generic functions that will begin collection of tables based on the
    // SIStandard.
    bool FindTransports();
    bool FindServices();
    bool FillPMap(SISTANDARD _SIStandard);

    bool AddPMT(uint16_t ServiceID);

    // Stops all collection of data and clears all values (on a channel change for example)
    void Reset();

    virtual void AddPid(uint16_t pid,uint8_t mask,uint8_t filter,bool CheckCRC,int bufferFactor);
    virtual void DelPid(int pid);
    virtual void DelAllPids();

    // Functions that may become signals for communication with the outside world
    void ServicesComplete();
    void GuideComplete();
//    void EventsReady();

    // Functions to get objects into other classes for manipulation
    bool GetTransportObject(NITObject& NIT);
    bool GetServiceObject(QMap_SDTObject& SDT);

    void ParseTable(uint8_t* buffer, int size, uint16_t pid);
    void CheckTrackers();

signals:

    void FindTransportsComplete();
    void FindServicesComplete();

    void FindEventsComplete();
    void EventsReady(QMap_Events* Events);

    void AllEventsPulled();

    void TableLoaded();

    void UpdatePMT(const PMTObject *pmt);

private:


    // Fixes for various DVB Network Spec Deviations
    void LoadDVBSpecDeviations(uint16_t NetworkID);

    bool PAT_ready;
    bool PMT_ready;

    // Timeout Variables
    QDateTime TransportSearchEndTime;
    QDateTime ServiceSearchEndTime;
    QDateTime EventSearchEndTime;

    void LoadPrivateTypes(uint16_t networkID);


    // MPEG Transport Parsers (ATSC and DVB)
    tablehead_t       ParseTableHead(uint8_t* buffer, int size);

    void ParsePAT     (tablehead_t* head, uint8_t* buffer, int size);
    void ParseCAT     (tablehead_t* head, uint8_t* buffer, int size);
    void ParsePMT     (tablehead_t* head, uint8_t* buffer, int size);

    void ProcessUnknownDescriptor(uint8_t *buffer, int size);

    // Common Helper Functions
    QString DecodeText(uint8_t *s, int length);

    // DVB Helper Parsers
    QDateTime ConvertDVBDate(uint8_t* dvb_buf);

    // DVB Table Parsers
    void ParseNIT       (tablehead_t* head, uint8_t* buffer, int size);
    void ParseSDT       (tablehead_t* head, uint8_t* buffer, int size);
    void ParseDVBEIT    (tablehead_t* head, uint8_t* buffer, int size);

    // Common Descriptor Parsers
    CAPMTObject ParseDescriptorCA(uint8_t* buffer, int size);

    // DVB Descriptor Parsers
    void ParseDescriptorNetworkName(uint8_t* buffer, int size, NetworkObject &n);
    void ParseDescriptorLinkage(uint8_t* buffer, int size, NetworkObject &n);
    void ParseDescriptorService(uint8_t* buffer, int size, SDTObject& s);
    void ParseDescriptorFrequencyList(uint8_t* buffer, int size,TransportObject& t);
    void ParseDescriptorUKChannelList(uint8_t* buffer, int size, QMap_uint16_t& numbers);
    TransportObject ParseDescriptorTerrestrialDeliverySystem(uint8_t* buffer, int size);
    TransportObject ParseDescriptorSatelliteDeliverySystem(uint8_t* buffer, int size);
    TransportObject ParseDescriptorCableDeliverySystem(uint8_t* buffer, int size);
    QString ParseDescriptorLanguage(uint8_t* buffer, int size);
    void ParseDescriptorTeletext(uint8_t* buffer, int size);
    void ParseDescriptorSubtitling(uint8_t* buffer, int size);
    void ProcessDescriptorHuffmanEventInfo(unsigned char *buf, unsigned int len, Event& e);
    QString ProcessDescriptorHuffmanText(unsigned char *buf,unsigned int len);
    QString ProcessDescriptorHuffmanTextLarge(unsigned char *buf,unsigned int len);

    // DVB EIT Table Descriptors
    QString ProcessContentDescriptor       (uint8_t *buf,int size);
    void ProcessShortEventDescriptor       (uint8_t *buf,int size,Event& e);
    void ProcessExtendedEventDescriptor    (uint8_t *buf,int size,Event& e);
    void ProcessComponentDescriptor        (uint8_t *buf,int size,Event& e);

    // ATSC Helper Parsers
    QDateTime ConvertATSCDate(uint32_t offset);
    QString ParseMSS(uint8_t* buffer, int size);

    // ATSC Table Parsers
    void ParseMGT       (tablehead_t* head, uint8_t* buffer, int size);
    void ParseVCT      (tablehead_t* head, uint8_t* buffer, int size);
    void ParseRRT       (tablehead_t* head, uint8_t* buffer, int size);
    void ParseATSCEIT   (tablehead_t* head, uint8_t* buffer, int size, uint16_t pid);
    void ParseETT       (tablehead_t* head, uint8_t* buffer, int size, uint16_t pid);
    void ParseSTT       (tablehead_t* head, uint8_t* buffer, int size);
    void ParseDCCT      (tablehead_t* head, uint8_t* buffer, int size);
    void ParseDCCSCT    (tablehead_t* head, uint8_t* buffer, int size);

    // ATSC Descriptor Parsers
    QString ParseATSCExtendedChannelName(uint8_t* buffer, int size);
    void ParseDescriptorATSCContentAdvisory(uint8_t* buffer, int size);

    // Common Variables
    int  SIStandard;

    uint16_t CurrentTransport;
    uint16_t RequestedServiceID;
    uint16_t RequestedTransportID;
    
    // Preferred languages and their priority
    QMap<QString, int>  LanguagePriority;

    // DVB Variables
    uint16_t NITPID;

    // ATSC Variables


    // TODO: Generalize these objects since they should work ok for ATSC as well
    // Storage Objects (DVB)
    QValueList<CAPMTObject>          CATList;
    NITObject                             NITList;


    // Storage Objects (ATSC)

    // Storage Objects (ATSC & DVB)
    QMap2D_Events            EventMapObject;
    QMap_Events             EventList;

    int ThreadRunning;

    // Mutex Locks
    // TODO: Lock Events, and Services, Transports, etc
    pthread_mutex_t pmap_lock;

    bool         exitParserThread;

    TableSourcePIDObject TableSourcePIDs;

    /* Huffman Text Decompression Routines */
    int HuffmanGetRootNode(uint8_t Input, uint8_t Table[]);
    bool HuffmanGetBit(uint8_t test[], uint16_t bit);
    QString HuffmanToQString(uint8_t test[], uint16_t size,uint8_t Table[]);

    int Huffman2GetBit ( int bit_index, unsigned char *byteptr );
    uint16_t Huffman2GetBits ( int bit_index, int bit_count, unsigned char *byteptr );
    int Huffman2ToQString(unsigned char *compressed, int length, int table, QString &Decompressed);

    bool ParserInReset;

    bool        standardChange;

    /* New tracking objects */

    TableHandler* Table[NumHandlers+1];
    privateTypes    PrivateTypes;
    bool        PrivateTypesLoaded;

    /* EIT Fix Up Functions */
    void EITFixUp(Event& event);
    void EITFixUpStyle1(Event& event);
    void EITFixUpStyle2(Event& event);
    void EITFixUpStyle3(Event& event);
    void EITFixUpStyle4(Event& event);


    //DVB category descriptions
    typedef struct 
    {         
        uint8_t id;
        char *desc;
    } description_table_rec;
    static description_table_rec description_table[];
    QMap<uint8_t,QString>  m_mapCategories;
    void initialiseCategories();
};

#define SIPARSER(args...) \
    VERBOSE(VB_SIPARSER, QString("SIParser: ") << args);
        

#endif // SIPARSER_H


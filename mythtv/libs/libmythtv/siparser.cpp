#include "../libmyth/mythcontext.h"
#include "siparser.h"
#include <qdatetime.h>
#include <qtextcodec.h>
#include <qregexp.h>
#include "dvbtypes.h"
#include "atsc_huffman.h"

/* needed for kenneths verification functions that need to be re-written*/
#define WORD(i,j)   ((i << 8) | j)

// Set EIT_DEBUG_SID to a valid serviceid to enable EIT debugging
// #define EIT_DEBUG_SID 1602

SIParser::description_table_rec SIParser::description_table[] = 
{ 
    { 0x10, "Movies" },
    { 0x11, "Movie - detective/thriller" },
    { 0x12, "Movie - adventure/western/war" },
    { 0x13, "Movie - science fiction/fantasy/horror" },
    { 0x14, "Movie - comedy" },
    { 0x15, "Movie - soap/melodrama/folkloric" },
    { 0x16, "Movie - romance" },
    { 0x17, "Movie - serious/classical/religious/historical movie/drama" },
    { 0x18, "Movie - adult movie/drama" },
    
    { 0x20, "News" },
    { 0x21, "news/weather report" },
    { 0x22, "news magazine" },
    { 0x23, "documentary" },
    { 0x24, "discussion/interview/debate" },
    
    { 0x30, "show/game Show" },
    { 0x31, "game show/quiz/contest" },
    { 0x32, "variety show" },
    { 0x33, "talk show" },
    
    { 0x40, "Sports" },
    { 0x41, "special events (Olympic Games, World Cup etc.)" },
    { 0x42, "sports magazines" },
    { 0x43, "football/soccer" },
    { 0x44, "tennis/squash" },
    { 0x45, "team sports (excluding football)" },
    { 0x46, "athletics" },
    { 0x47, "motor sport" },
    { 0x48, "water sport" },
    { 0x49, "winter sports" },
    { 0x4A, "equestrian" },
    { 0x4B, "martial sports" },
    
    { 0x50, "Kids" },
    { 0x51, "pre-school children's programmes" },
    { 0x52, "entertainment programmes for 6 to14" },
    { 0x53, "entertainment programmes for 10 to 16" },
    { 0x54, "informational/educational/school programmes" },
    { 0x55, "cartoons/puppets" },
    
    { 0x60, "music/ballet/dance" },
    { 0x61, "rock/pop" },
    { 0x62, "serious music/classical music" },
    { 0x63, "folk/traditional music" },
    { 0x64, "jazz" },
    { 0x65, "musical/opera" },
    { 0x66, "ballet" },

    { 0x70, "arts/culture" },
    { 0x71, "performing arts" },
    { 0x72, "fine arts" },
    { 0x73, "religion" },
    { 0x74, "popular culture/traditional arts" },
    { 0x75, "literature" },
    { 0x76, "film/cinema" },
    { 0x77, "experimental film/video" },
    { 0x78, "broadcasting/press" },
    { 0x79, "new media" },
    { 0x7A, "arts/culture magazines" },
    { 0x7B, "fashion" },
    
    { 0x80, "social/policical/economics" },
    { 0x81, "magazines/reports/documentary" },
    { 0x82, "economics/social advisory" },
    { 0x83, "remarkable people" },
    
    { 0x90, "Education/Science/Factual" },
    { 0x91, "nature/animals/environment" },
    { 0x92, "technology/natural sciences" },
    { 0x93, "medicine/physiology/psychology" },
    { 0x94, "foreign countries/expeditions" },
    { 0x95, "social/spiritual sciences" },
    { 0x96, "further education" },
    { 0x97, "languages" },
    
    { 0xA0, "leisure/hobbies" },
    { 0xA1, "tourism/travel" },
    { 0xA2, "handicraft" },
    { 0xA3, "motoring" },
    { 0xA4, "fitness & health" },
    { 0xA5, "cooking" },
    { 0xA6, "advertizement/shopping" },
    { 0xA7, "gardening" },
    // Special
    { 0xB0, "Original Language" },
    { 0xB1, "black & white" },
    { 0xB2, "unpublished" },
    { 0xB3, "live broadcast" },
    // UK Freeview custom id
    { 0xF0, "Drama" },
    { 0, NULL }    
};

SIParser::SIParser()
{
    ThreadRunning = false;
    pthread_mutex_init(&pmap_lock, NULL);
    standardChange = false;
    SIStandard = SI_STANDARD_AUTO;

    /* Set the PrivateTypes to default values */
    PrivateTypesLoaded = false;
    PrivateTypes.reset();

    /* Initialize the Table Handlers */
    Table[PAT] = new PATHandler();
    Table[PMT] = new PMTHandler();
    Table[MGT] = new MGTHandler();
    Table[STT] = new STTHandler();
    Table[EVENTS] = new EventHandler();
    Table[SERVICES] = new ServiceHandler();
    Table[NETWORK] = new NetworkHandler();

    initialiseCategories();

    Reset();

    // Get a list of wanted languages and set up their priorities
    // (Lowest number wins)
    QStringList PreferredLanguages = QStringList::split(",", gContext->GetSetting("PreferredLanguages", ""));
    QStringList::Iterator plit;
    int prio = 1;
    for (plit = PreferredLanguages.begin(); plit != PreferredLanguages.end(); ++plit)
    {
        SIPARSER(QString("Added preferred language %1 with priority %2").arg(*plit).arg(prio));
        LanguagePriority[*plit] = prio++;
    }
}

SIParser::~SIParser()
{
    pthread_mutex_destroy(&pmap_lock);
}

void SIParser::initialiseCategories()
{
    description_table_rec *p=description_table;
    while (p->id!= 0)
    { 
        m_mapCategories[p->id] = QObject::tr(p->desc);
        p++;
    }
}

/* Resets all trackers, and closes all section filters */
void SIParser::Reset()
{

    ParserInReset = true;

    // Close all pids that are open

    SIPARSER("About to do a reset");

    Table[MGT]->Reset();

    SIPARSER("Closing all PIDs");
    DelAllPids();

    PrivateTypesLoaded = false;
    PrivateTypes.reset();

    SIPARSER("Resetting all Table Handlers");
    pthread_mutex_lock(&pmap_lock);

    for (int x = 0; x < NumHandlers ; x++)
        Table[x]->Reset();

    pthread_mutex_unlock(&pmap_lock);

    SIPARSER("SIParser Reset due to channel change");

}

void SIParser::CheckTrackers()
{

    uint16_t pid;
    uint8_t filter,mask;

    pthread_mutex_lock(&pmap_lock);

    /* Check Dependencys and update if necessary */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->Complete())
        {
            SIPARSER(QString("Table[%1]->Complete() == true").arg((tabletypes) x));
            for (int y = 0 ; y < NumHandlers ; y++)
                Table[y]->DependencyMet((tabletypes) x);
// TODO: Emit completion here for tables to siscan
        }
    }

    /* Handle opening any PIDs in this loop only */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->RequirePIDs())
        {
            SIPARSER(QString("Table[%1]->RequirePIDs() == true").arg((tabletypes) x));
            while (Table[x]->GetPIDs(pid,filter,mask))
            {
                AddPid(pid, mask, filter, true, 
                        ((SIStandard == SI_STANDARD_DVB) && (x == EVENTS)) ? 1000 : 10);
            }
        }
    }

    uint16_t key0 = 0,key1 = 0;

    /* See if any new objects require an emit */
    for (int x = 0 ; x < NumHandlers ; x++)
    {
        if (Table[x]->EmitRequired())
        {
            SIPARSER(QString("Table[%1]->EmitRequired() == true").arg((tabletypes) x));
            switch (x)
            {
                case PMT:
                             while (Table[PMT]->GetEmitID(key0,key1))
                                 emit NewPMT( ((PMTHandler *) Table[PMT])->pmt[key0] );
                             break;
                case EVENTS:
                             while (Table[EVENTS]->GetEmitID(key0,key1))
                                 emit EventsReady( &((EventHandler *) Table[EVENTS])->Events[key0] );
                             break;
                case NETWORK:
                             while(Table[NETWORK]->GetEmitID(key0,key1))
                                 emit FindTransportsComplete();

                default:
                             break;
            }
        }
    }

    pthread_mutex_unlock(&pmap_lock);


}

void SIParser::LoadPrivateTypes(uint16_t NetworkID)
{
    QString STD;

    switch (SIStandard)
    {
        case SI_STANDARD_DVB:    STD = "dvb";
                                 break;
        case SI_STANDARD_ATSC:   STD = "atsc";
                                 break;
        /* If you don't know the SI Standard yet you need to bail out */
        default:
                                 return;
    }

    QString theQuery = QString("select private_type,private_value from dtv_privatetypes where "
                       "networkid = %1 and sitype = \"%2\"")
                       .arg(NetworkID)
                       .arg(STD);

    pthread_mutex_lock(db_lock);
    QSqlQuery query = db_conn->exec(theQuery);

    if(!query.exec(theQuery))
        MythContext::DBError("Loading Private Types for SIParser", query);

    if (!query.isActive())
        MythContext::DBError("Loading Private Types for SIParser", query);

    if (query.numRowsAffected() > 0)
    {
        query.next();
        for (int x = 0 ; x < query.numRowsAffected() ; x++)
        {
            SIPARSER( QString("Private Type %1 = %2 defined for NetworkID %3")
                     .arg(query.value(0).toString())
                     .arg(query.value(1).toString())
                     .arg(NetworkID) );

            if (QString(query.value(0).toString()) == "sdt_mapping")
            {
                PrivateTypes.SDTMapping = true;
                SIPARSER("SDT Mapping Incorrect for this Service Fixup Loaded");
            }
            if (QString(query.value(0).toString()) == "channel_numbers")
            {
                PrivateTypes.ChannelNumbers = query.value(1).toInt();
                SIPARSER(QString("ChannelNumbers Present using Descriptor %d")
                         .arg(PrivateTypes.ChannelNumbers));
            }
            if (QString(query.value(0).toString()) == "force_guide_present")
            {
                if (query.value(1).toString() == "yes")
                {
                    PrivateTypes.ForceGuidePresent = true;
                    SIPARSER(QString("Forcing Guide Present"));
                }
            }
            if (QString(query.value(0).toString()) == "guide_fixup")
            {
                PrivateTypes.EITFixUp = query.value(1).toInt();
                SIPARSER(QString("Using Guide Fixup Scheme #%1").arg(PrivateTypes.EITFixUp));
            }
            if (QString(query.value(0).toString()) == "guide_ranges")
            {
                PrivateTypes.CustomGuideRanges = true;
                QStringList temp  = QStringList::split(",",query.value(1).toString());
                PrivateTypes.CurrentTransportTableMin = temp[0].toInt();
                PrivateTypes.CurrentTransportTableMax = temp[1].toInt();
                PrivateTypes.OtherTransportTableMin = temp[2].toInt();
                PrivateTypes.OtherTransportTableMax = temp[3].toInt();
                
                SIPARSER(QString("Using Guide Custom Range; CurrentTransport: %1-%2, OtherTransport: %3-%4")
                         .arg(PrivateTypes.CurrentTransportTableMin,2,16)
                         .arg(PrivateTypes.CurrentTransportTableMax,2,16)
                         .arg(PrivateTypes.OtherTransportTableMin,2,16)
                         .arg(PrivateTypes.OtherTransportTableMax,2,16));
            }
            if (QString(query.value(0).toString()) == "tv_types")
            {
                PrivateTypes.TVServiceTypes.clear();
                QStringList temp  = QStringList::split(",",query.value(1).toString());
                QStringList::Iterator i;
                for (i = temp.begin() ; i != temp.end() ; i++)
                {
                    PrivateTypes.TVServiceTypes[(*i).toInt()] = 1;
                    SIPARSER(QString("Added TV Type %1").arg((*i).toInt()));
                }
            }
            query.next();
        }
    }
    else
        SIPARSER(QString("No Private Types defined for NetworkID %1").arg(NetworkID));

    pthread_mutex_unlock(db_lock);

    PrivateTypesLoaded = true;
}

bool SIParser::GetTransportObject(NITObject& NIT)
{
    pthread_mutex_lock(&pmap_lock);
    NIT = ((NetworkHandler*) Table[NETWORK])->NITList;
    pthread_mutex_unlock(&pmap_lock);
    return true;
}

bool SIParser::GetServiceObject(QMap_SDTObject& SDT)
{

    pthread_mutex_lock(&pmap_lock);
    SDT = ((ServiceHandler*) Table[SERVICES])->Services[0];
    pthread_mutex_unlock(&pmap_lock);
    return true;
}

void SIParser::AddPid(uint16_t pid,uint8_t mask,uint8_t filter, bool CheckCRC, int bufferFactor)
{
    (void) pid;
    (void) mask;
    (void) filter;
    (void) CheckCRC;
    (void) bufferFactor;
    SIPARSER("Using AddPid from SIParser which does nothing");
}

void SIParser::DelPid(int pid)
{
    (void) pid;
    SIPARSER("Using DelPid from SIParser which does nothing");
}

void SIParser::DelAllPids()
{
    SIPARSER("Using DelAllPids from SIParser which does nothing");
}

bool SIParser::FillPMap(SISTANDARD _SIStandard)
{

    pthread_mutex_lock(&pmap_lock);
    SIPARSER("Requesting PAT");

    /* By default open only the PID for PAT */
    Table[PAT]->Request(0);
    Table[SERVICES]->Request(0);
    Table[MGT]->Request(0);
    Table[STT]->Request(0);
    Table[NETWORK]->Request(0);

    for (int x = 0 ; x < NumHandlers ; x++)
        Table[x]->SetSIStandard(_SIStandard);

    SIStandard = _SIStandard;

    pthread_mutex_unlock(&pmap_lock);

    return true;
}

bool SIParser::AddPMT(uint16_t ServiceID)
{

    pthread_mutex_lock(&pmap_lock);
    SIPARSER(QString("Adding a PMT (%1) to the request list").arg(ServiceID));
    Table[PMT]->RequestEmit(ServiceID);
    pthread_mutex_unlock(&pmap_lock);

    return true;
}

bool SIParser::FindTransports()
{
    Table[NETWORK]->RequestEmit(0);
    return true;
}

bool SIParser::FindServices()
{
    Table[SERVICES]->RequestEmit(0);
    return true;
}

/*------------------------------------------------------------------------
 *   COMMON PARSER CODE
 *------------------------------------------------------------------------*/

void SIParser::ParseTable(uint8_t* buffer, int size, uint16_t pid)
{

    pthread_mutex_lock(&pmap_lock);

#ifndef USING_DVB_EIT
    (void) pid;
#endif

    tablehead_t head = ParseTableHead(buffer,size);

    /* Parse Common Tables (PAT/CAT/PMT) regardless of SI Standard */
    switch(head.table_id)
    {
        case 0x00:  ParsePAT(&head, &buffer[8], size-8);
                    break;

        case 0x01:  ParseCAT(&head, &buffer[8], size-8);
                    break;

        case 0x02:  ParsePMT(&head, &buffer[8], size-8);
                    break;

    }

    /* In Detection mode determine what SIStandard you are using */
    if (SIStandard == SI_STANDARD_AUTO)
    {
        switch(head.table_id)
        {
        case 0x46:
        case 0x42:  
            SIStandard = SI_STANDARD_DVB;
            SIPARSER("SI Standard Detected: DVB");
            standardChange = true;
        break;
        case 0xC7:  
            SIStandard = SI_STANDARD_ATSC;
            SIPARSER("SI Standard Detected: ATSC");
            standardChange = true;
        break;
        }
    }

    /* Parse DVB Specific Tables Here */
    if (SIStandard == SI_STANDARD_DVB)
    {
        switch(head.table_id)
        {
        case 0x40:
            /* Network Information Table(s) */
            ParseNIT(&head, &buffer[8], size-8);
            break;
        case 0x42: /* Service Table(s) */
        case 0x46:  
            ParseSDT(&head, &buffer[8], size-8);
            break;
#ifdef USING_DVB_EIT
        case 0x50 ... 0x6F:
            /* Standard Future Event Information Table(s) */
            ParseDVBEIT(&head, &buffer[8], size-8);
            break;
#endif
      
        }
    }

    if (SIStandard == SI_STANDARD_ATSC)
    {
        switch(head.table_id)
        {
              case 0xC7:  ParseMGT(&head, &buffer[8], size-8);
                          break;
              case 0xC8:  
              case 0xC9:  ParseVCT(&head, &buffer[8], size-8);
                          break;
              case 0xCA:  ParseRRT(&head, &buffer[8], size-8);
                          break;
#ifdef USING_DVB_EIT
              case 0xCB:  ParseATSCEIT(&head, &buffer[8], size-8, pid);
                          break;
              case 0xCC:  ParseETT(&head, &buffer[8], size-8, pid);
                          break;
#endif
              case 0xCD:  ParseSTT(&head, &buffer[8], size-8);
                          break;
              case 0xD3:  ParseDCCT(&head, &buffer[8], size-8);
                          break;
              case 0xD4:  ParseDCCSCT(&head, &buffer[8], size-8);
                          break;
        }
    }

    pthread_mutex_unlock(&pmap_lock);

    return;
}

tablehead_t SIParser::ParseTableHead(uint8_t* buffer, int size)
{
// TODO: Maybe handle the size but should be OK if CRC passes

    (void) size;
    tablehead_t head;

    head.table_id       = buffer[0];
    head.section_length = ((buffer[1] & 0x0f)<<8) | buffer[2];
    head.table_id_ext   = (buffer[3] << 8) | buffer[4];
    head.current_next   = (buffer[5] & 0x1);
    head.version        = ((buffer[5] >> 1) & 0x1f);
    head.section_number = buffer[6];
    head.section_last   = buffer[7];

    return head;

}

void SIParser::ParsePAT(tablehead_t* head,uint8_t *buffer,int size)
{

    // Check to see if you have already loaded all of the PAT sections
    // ISO 13818-1 state that PAT can be segmented, although it rarely is
    if (!verifyPAT(head,buffer,size))
    {
        SIPARSER("Malformed PAT!");
        return;
    }

    if(((PATHandler*) Table[PAT])->Tracker.AddSection(head))
        return;

    SIPARSER(QString("PAT Version = %1").arg(head->version));
    PrivateTypes.CurrentTransportID = head->table_id_ext;
    SIPARSER(QString("Tuned to TransportID: %1").arg(PrivateTypes.CurrentTransportID));

    int pos = -1;
    while (pos < (size - 4))
    {
        if (pos+4 >= (size-4))
        {
            break;
        }
        uint16_t program = (buffer[pos + 1] << 8) | buffer[pos + 2];
        uint16_t pid = ((buffer[pos + 3] & 0x1f)<<8) | buffer[pos + 4];
        if (program != 0)
        {
            ((PATHandler*) Table[PAT])->pids[program]=pid;
            Table[PMT]->AddKey(pid,0);
            ((PMTHandler*) Table[PMT])->pmt[program].PMTPID = pid;
        }
        else
        {
            // Program 0 in the PAT represents the location of the NIT in DVB
            NITPID = pid;
            SIPARSER(QString("NIT Present on this transport on PID %1").arg(NITPID));
        }
        pos += 4;

    }


    QString ProgramList = QString("Services on this Transport: ");
    QMap_uint16_t::Iterator p;

    for (p = ((PATHandler*) Table[PAT])->pids.begin(); p != ((PATHandler*) Table[PAT])->pids.end() ; ++p)
        ProgramList += QString("%1 ").arg(p.key());
    SIPARSER(ProgramList);


}

void SIParser::ParseCAT(tablehead_t* head, uint8_t* buffer, int size)
{
    (void) head;

    CAPMTObject c;

    int pos = -1;
    while (pos < (size - 4))
    {
        if (buffer[pos+1] == 0x09)
        {
            c = ParseDescriptorCA(&buffer[pos+1],buffer[pos+2]);
            SIPARSER(QString("CA System 0x%1, EMM PID = %2")
                     .arg(c.CASystemID,0,16)
                     .arg(c.PID));
        }
        pos += buffer[pos+2] + 2;
    }

}

void SIParser::ParsePMT(tablehead_t* head, uint8_t* buffer, int size)
{
    // TODO: Catch serviceMove descriptor and send a signal when you get one
    //       to retune to correct transport or send an error tuning the channel

    if (!verifyPMT(buffer,size))
    {
        SIPARSER("Malformed PMT!");
        return;
    }

    if (Table[PMT]->AddSection(head,head->table_id_ext,0))
        return;

    SIPARSER(QString("PMT ServiceID: %1 Version = %2").arg(head->table_id_ext).arg(head->version));

    // Create a PMTObject and populate it
    PMTObject p;
    uint8_t descriptors_length;
    uint16_t pidescriptors_length = (((buffer[2] & 0x0F) << 8) | buffer[3]);

    p.PCRPID = (buffer[0] & 0x1F) << 8 | buffer[1];
    p.ServiceID = head->table_id_ext;

    // Process Program Info Descriptors
    uint16_t pos = 4;
    uint16_t tempPos = pos + pidescriptors_length;
    while (pos < tempPos)
    {
        switch (buffer[pos])
        {
            // Conditional Access Descriptor
            case 0x09:
                {
                    CAPMTObject cad = ParseDescriptorCA(&buffer[pos], buffer[pos+1]);
                    p.CA.insert(cad.CASystemID, cad);
                    p.hasCA = true;
                }
                break;

            default:
                SIPARSER(QString("Unknown descriptor, tag = %1").arg(buffer[pos]));
                p.Descriptors.append(Descriptor(&buffer[pos], buffer[pos + 1] + 2));
                break;
        }
        pos += buffer[pos+1] + 2;
    }


    // Process streams
    while ((pos + 4) < size)
    {

        ElementaryPIDObject e;

        // Reset Elementary PID object
        e.Reset();

        // Grab PIDs, and set PID Type
        e.PID = (buffer[pos+1] & 0x1F) << 8 | buffer[pos+2];
        e.Orig_Type = buffer[pos];
        SIPARSER(QString("PID: %1").arg(e.PID));

        // The stream type can be detected in two ways:
        // - by stream type field in PMT, if known
        // - by a descriptor
        switch (e.Orig_Type)
        {
            case 0x01:
                e.Type = ES_TYPE_VIDEO_MPEG1;
                break;
            case 0x02:
                e.Type = ES_TYPE_VIDEO_MPEG2;
                break;
            case 0x03:
                e.Type = ES_TYPE_AUDIO_MPEG1;
                break;
            case 0x04:
                e.Type = ES_TYPE_AUDIO_MPEG2;
                break;
            case 0x08:
            case 0x0B:
                e.Type = ES_TYPE_DATA;
                break;
            case 0x0F:
                e.Type = ES_TYPE_AUDIO_AAC;
                break;
            case 0x81:
                e.Type = ES_TYPE_AUDIO_AC3;// Where ATSC Puts the AC3 Descriptor
                break;

        }

        descriptors_length = (buffer[pos+3] & 0x0F) << 8 | buffer[pos+4];
        pos += 5;
        tempPos = pos + descriptors_length;

        while (pos < tempPos)
        {
            uint8_t *descriptor = &buffer[pos];
            int descriptor_tag = descriptor[0];
            int descriptor_len = descriptor[1];

            if (descriptor_tag == 0x09) // Conditional Access Descriptor
            {
                // Note: the saved streams have already been descrambled by the CAM
                // so any CA descriptors should *not* be added to the descriptor list.
                // We need a CAPMTObject to send to the CAM though.
                CAPMTObject cad = ParseDescriptorCA(descriptor, descriptor_len);
                e.CA.insert(cad.CASystemID, cad);
                p.hasCA = true;
            }
            else
            {
                e.Descriptors.append(Descriptor(descriptor, descriptor_len + 2));

                switch (descriptor_tag)
                {
                    case 0x0A: // ISO 639 Language Descriptor
                        e.Language = ParseDescriptorLanguage(descriptor, descriptor_len);
                        break;

                    case 0x56: // Teletext Descriptor
                        ParseDescriptorTeletext(descriptor, descriptor_len);
                        e.Type = ES_TYPE_TELETEXT;
                        break;

                    case 0x59: // Subtitling Descriptor
                        ParseDescriptorSubtitling(descriptor, descriptor_len);
                        e.Type = ES_TYPE_SUBTITLE;
                        break;

                    case 0x6A: // AC3 Descriptor
                        SIPARSER(QString("AC3 Descriptor"));
                        e.Type = ES_TYPE_AUDIO_AC3;
                        break;

                    default:
                        SIPARSER(QString("Unknown descriptor, tag = %1").arg(descriptor_tag));
                        break;
                }
            }
            
            pos += descriptor_len + 2;
        }

        switch (e.Type)
        {
            case ES_TYPE_VIDEO_MPEG1:
                e.Description = QString("MPEG-1 Video");
                p.hasVideo = true;
                break;
            case ES_TYPE_VIDEO_MPEG2:
                e.Description = QString("MPEG-2 Video");
                p.hasVideo = true;
                break;
            case ES_TYPE_AUDIO_MPEG1:
                e.Description = QString("MPEG-1 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_MPEG2:
                e.Description = QString("MPEG-2 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_AC3:
                e.Description = QString("AC3 Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_DTS:
                e.Description = QString("DTS Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_AUDIO_AAC:
                e.Description = QString("AAC Audio");
                p.hasAudio = true;
                break;
            case ES_TYPE_TELETEXT:
                e.Description = QString("Teletext");
                break;
            case ES_TYPE_SUBTITLE:
                e.Description = QString("Subtitle");
                break;
            case ES_TYPE_DATA:
                e.Description = QString("Data");
                break;
            default:
                e.Description = QString("Unknown type: %1").arg(e.Orig_Type);
                break;
        }

        p.Components += e;
    }

    ((PMTHandler*) Table[PMT])->pmt[head->table_id_ext] = p;
}

void SIParser::ProcessUnknownDescriptor(uint8_t* buf, int len)
{

    QString temp = "Unknown Descriptor: ";
   
    for (int x=0;x<len;x++)
       temp += QString("%1 ").arg(buf[x],2,16);

    SIPARSER(temp);

}

/*------------------------------------------------------------------------
 *   COMMON HELPER FUNCTIONS
 *------------------------------------------------------------------------*/

// Decode a text string according to ETSI EN 300 468 Annex A
QString SIParser::DecodeText(uint8_t *s, int length)
{
    QString result;

    if (length <= 0)
        return QString("");

    if ((*s < 0x10) || (*s >= 0x20))
    {
        // Strip formatting characters
        for (int p = 0; p < length; p++)
            if ((s[p] >= 0x80) && (s[p] <= 0x9F))
                memmove(s + p, s + p + 1, --length - p);

        if (*s >= 0x20)
        {
            result = QString::fromLatin1((const char*) s, length);
        }
        else if ((*s >= 0x01) && (*s <= 0x0B))
        {
            QString coding = "ISO8859-" + QString::number(4 + *s);
            QTextCodec *codec = QTextCodec::codecForName(coding);
            result = codec->toUnicode((const char*) s + 1, length - 1);
        }
        else
        {
            // Unknown/invalid encoding - assume local8Bit
            result = QString::fromLocal8Bit((const char*) s + 1, length - 1);
        }
    }
    else
    {
        // TODO: Handle multi-byte encodings

        SIPARSER("Multi-byte coded text - not yet supported!");
        result = "N/A";
    }

    return result;
}

/*------------------------------------------------------------------------
 *   DVB HELPER FUNCTIONS
 *------------------------------------------------------------------------*/

QDateTime SIParser::ConvertDVBDate(uint8_t* dvb_buf)
{
// TODO: clean this up some since its sort of a mess right now

    // This function taken from dvbdate.c in linuxtv-apps code

    int i;
    int year, month, day, hour, min, sec;
    long int mjd;

    mjd = (dvb_buf[0] & 0xff) << 8;
    mjd += (dvb_buf[1] & 0xff);
    hour = bcdtoint(dvb_buf[2] & 0xff);
    min = bcdtoint(dvb_buf[3] & 0xff);
    sec = bcdtoint(dvb_buf[4] & 0xff);

/*
 * Use the routine specified in ETSI EN 300 468 V1.4.1,
 * "Specification for Service Information in Digital Video Broadcasting"
 * to convert from Modified Julian Date to Year, Month, Day.
 */

    year = (int) ((mjd - 15078.2) / 365.25);
    month = (int) ((mjd - 14956.1 - (int) (year * 365.25)) / 30.6001);
    day = mjd - 14956 - (int) (year * 365.25) - (int) (month * 30.6001);
    if (month == 14 || month == 15)
        i = 1;
    else
        i = 0;
    year += i;
    month = month - 1 - i * 12;

    year += 1900;

    QDateTime UTCTime = QDateTime(QDate(year,month,day),QTime(hour,min,sec));

    // Convert to localtime
    QDateTime EPOCTime = QDateTime(QDate(1970, 1, 1));
    int timesecs = EPOCTime.secsTo(UTCTime);

    QDateTime LocalTime;

    LocalTime.setTime_t(timesecs);

    QString UTCText = UTCTime.toString();
    QString LocalText = LocalTime.toString();

    return LocalTime;

}

/*------------------------------------------------------------------------
 *   DVB TABLE PARSERS
 *------------------------------------------------------------------------*/


void SIParser::ParseNIT(tablehead_t* head, uint8_t* buffer, int size)
{
    // Only process current network NITs for now
    if (head->table_id != 0x40)
        return;

    // Check to see if you already pulled this table section
    if (Table[NETWORK]->AddSection(head,0,0))
        return;

    // TODO: Emit a table load here for scanner

    TransportObject t;

    QMap_uint16_t ChannelNumbers;

    uint16_t network_descriptors_length  = (buffer[0] & 0x0F) << 8 | buffer[1];
    uint16_t transport_stream_loop_length = 0;
    uint16_t transport_descriptors_length = 0;

    uint16_t pos = 2;

    // Parse Network Descriptors (Name, Linkage)
    if (network_descriptors_length > 0)
    {
       // Create a Network Object
       NetworkObject n;
       while ((network_descriptors_length) > (pos-2)) {

           switch (buffer[pos])
           {
               case 0x40:
                          ParseDescriptorNetworkName(&buffer[pos],buffer[pos+1],n);
                          break;
               case 0x4A:
                          ParseDescriptorLinkage(&buffer[pos],buffer[pos+1],n);
                          break;
               default:
                          ProcessUnknownDescriptor(&buffer[pos],buffer[pos+1]);
                          break;
           }
           pos += (buffer[pos+1] + 2);
       }
       ((NetworkHandler*) Table[NETWORK])->NITList.Network += n;

    }

    transport_stream_loop_length = (buffer[pos] & 0x0F) << 8 | buffer[pos+1];
    pos += 2;

    // transport desctiptors parser
    uint16_t dpos = 0;

    // Safe to assume that you can run until the end
    while (pos < size - 4) {

        if (PrivateTypesLoaded == false)
        {
            t.NetworkID = buffer[pos+2] << 8 | buffer[pos+3];
            LoadPrivateTypes(t.NetworkID);
        }

        transport_descriptors_length = (buffer[pos+4] & 0x0F) << 8 | buffer[pos+5];
        dpos=0;
        while ((transport_descriptors_length) > (dpos))
        {
            switch (buffer[pos + 6 + dpos]) {

               // DVB-C - Descriptor Parser written by Ian Caulfield
               case 0x44:
                            t = ParseDescriptorCableDeliverySystem(
                                    &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                            break;

               // DVB-T - Verified thanks to adante in #mythtv allowing me access to his DVB-T card
               case 0x5A:
                            t = ParseDescriptorTerrestrialDeliverySystem(
                                    &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                            break;
               // DVB-S
               case 0x43:
                            t = ParseDescriptorSatelliteDeliverySystem(
                                    &buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                            break;
               case 0x62:
                            ParseDescriptorFrequencyList(&buffer[pos+6+dpos],buffer[pos + 7 + dpos],t);
                            break;
               case 0x83:
                            if (PrivateTypes.ChannelNumbers == 0x83)
                                ParseDescriptorUKChannelList(&buffer[pos+6+dpos],buffer[pos + 7 + dpos],
                                                             ChannelNumbers);
                            break;
               default:
                            ProcessUnknownDescriptor(&buffer[pos + 6 + dpos],buffer[pos + 7 + dpos]);
                            break;
             }
             dpos += (buffer[pos + 7 + dpos] + 2);
        }

        // Get TransportID and NetworkID
        t.TransportID = buffer[pos] << 8 | buffer[pos+1];
        t.NetworkID = buffer[pos+2] << 8 | buffer[pos+3];

        Table[SERVICES]->Request(t.TransportID);
    
        if (PrivateTypes.ChannelNumbers && !(ChannelNumbers.empty()))
        {
            QMap_uint16_t::Iterator c;
            for (c = ChannelNumbers.begin() ; c != ChannelNumbers.end() ; ++c)
                ((ServiceHandler*) Table[SERVICES])->Services[t.TransportID][c.key()].ChanNum = c.data();
        }

        ((NetworkHandler*) Table[NETWORK])->NITList.Transport += t;

        pos += transport_descriptors_length + 6;
    }

}

void SIParser::ParseSDT(tablehead_t* head, uint8_t* buffer, int size)
{

    /* Signal to keep scan wizard bars moving */
    emit TableLoaded();

    int CurrentTransport = 0;

    uint16_t network_id = buffer[0] << 8 | buffer[1];
    // TODO: Handle Network Specifics here if they aren't set

    if (PrivateTypesLoaded == false)
        LoadPrivateTypes(network_id);

    if (PrivateTypes.SDTMapping)
    {
        if (PrivateTypes.CurrentTransportID == head->table_id_ext)
            CurrentTransport = 1;
    }
    else
    {
        if (head->table_id == 0x42)
            CurrentTransport = 1;
    }

    if (CurrentTransport)
    {
        if (Table[SERVICES]->AddSection(head,0,0))
            return;
    }
    else
    {
        if (Table[SERVICES]->AddSection(head,head->table_id_ext,0))
            return;
    }

    uint16_t len = 0;
    uint16_t pos = 3;
    uint16_t lentotal = 0;
    uint16_t descriptors_loop_length = 0;
    SDTObject s;

    SIPARSER(QString("SDT: NetworkID=%1 TransportID=%2").arg(network_id).arg(head->table_id_ext));

    while (pos < (size-4))
    {
        s.ServiceID = buffer[pos] << 8 | buffer[pos+1];
        s.TransportID = head->table_id_ext;
        s.NetworkID = network_id;
        s.EITPresent = PrivateTypes.ForceGuidePresent ? 1 : (buffer[pos+2] & 0x02) >> 1;
        s.RunningStatus = (buffer[pos+3] & 0xE0) >> 5;
        s.CAStatus = (buffer[pos+3] & 0x10) >> 4;
        s.Version = head->version;

        if(((ServiceHandler*) Table[SERVICES])->Services[s.TransportID].contains(s.ServiceID))
            s.ChanNum = ((ServiceHandler*) Table[SERVICES])->Services[s.TransportID][s.ServiceID].ChanNum;

        descriptors_loop_length = (buffer[pos+3] & 0x0F) << 8 | buffer[pos+4];
        lentotal = 0;

        while ((descriptors_loop_length) > (lentotal))
        {
            switch(buffer[pos + 5 + lentotal])
            {
            case 0x48:
                ParseDescriptorService(&buffer[pos + 5 + lentotal], 
                     buffer[pos + 6 + lentotal], s);
                break;
            default:
                ProcessUnknownDescriptor(&buffer[pos + 5 + lentotal],
                      buffer[pos + 6 + lentotal]);
                break;
            }
            len = buffer[pos + 6 + lentotal];
            lentotal += (len + 2);
        }

        bool eit_requested = false;
#ifdef USING_DVB_EIT

        if ((s.EITPresent) && (s.ServiceType == 1) && ((!PrivateTypes.GuideOnSingleTransport) ||
            ((PrivateTypes.GuideOnSingleTransport) && 
            (PrivateTypes.GuideTransportID == PrivateTypes.CurrentTransportID)))) 
        {
            Table[EVENTS]->RequestEmit(s.ServiceID);
            eit_requested = true;
        }
#endif

        SIPARSER(QString("SDT: sid=%1 type=%2 eit_present=%3 eit_requested=%4 name=%5")
                 .arg(s.ServiceID).arg(s.ServiceType)
                 .arg(s.EITPresent).arg(eit_requested)
                 .arg(s.ServiceName.ascii()));

        if (CurrentTransport)
            ((ServiceHandler*) Table[SERVICES])->Services[0][s.ServiceID] = s;
        else
            ((ServiceHandler*) Table[SERVICES])->Services[s.TransportID][s.ServiceID] = s;
        s.Reset();  
        pos += (descriptors_loop_length + 5);
    }

    if (CurrentTransport)
        emit FindServicesComplete();

    // TODO: This is temp
    Table[EVENTS]->DependencyMet(SERVICES);
    Table[EVENTS]->AddPid(0x12,0x00,0x00,true);
}

void SIParser::ParseDVBEIT(tablehead_t* head, uint8_t* buffer ,int size)
{

    uint8_t last_segment_number  = buffer[4];
    uint8_t last_table_id        = buffer[5];

    if (!((EventHandler*) Table[EVENTS])->TrackerSetup[head->table_id_ext])
    {
        if (PrivateTypes.CustomGuideRanges)
        {
            if ((head->table_id >= PrivateTypes.CurrentTransportTableMin)
                && (head->table_id <= PrivateTypes.CurrentTransportTableMax))
            {
                for (int x = PrivateTypes.CurrentTransportTableMin; x <= PrivateTypes.CurrentTransportTableMax; x++)
                    ((EventHandler*) Table[EVENTS])->Tracker[head->table_id_ext][x].Reset();
            }
            else if ((head->table_id >= PrivateTypes.OtherTransportTableMin)
                && (head->table_id <= PrivateTypes.OtherTransportTableMax))
            {
                for (int x = PrivateTypes.OtherTransportTableMin; x <= PrivateTypes.OtherTransportTableMax; x++)
                    ((EventHandler*) Table[EVENTS])->Tracker[head->table_id_ext][x].Reset();
            }
            
        }
        else
        {
            if ((head->table_id & 0xF0) == 0x50)
            {
                for (int x = 0x50 ; x < (last_table_id & 0x0F) + 0x50 ; x++)
                   ((EventHandler*) Table[EVENTS])->Tracker[head->table_id_ext][x].Reset();
            }

            if ((head->table_id & 0xF0) == 0x60)
            {
                for (int x = 0x60 ; x < (last_table_id & 0x0F) + 0x60 ; x++)
                    ((EventHandler*) Table[EVENTS])->Tracker[head->table_id_ext][x].Reset();
            }
        }
        ((EventHandler*) Table[EVENTS])->TrackerSetup[head->table_id_ext] = true;
    }

    if (Table[EVENTS]->AddSection(head,head->table_id_ext,head->table_id))
        return;

    if (last_segment_number != head->section_last)
    {
        for (int x=(last_segment_number+1);x<((head->section_number&0xF8)+8);x++)
            ((EventHandler*) Table[EVENTS])->Tracker[head->table_id_ext][head->table_id].MarkUnused(x);

    }

    uint16_t pos = 6;
    uint16_t des_pos = 0;
    uint16_t descriptor_length = 0;

    // Event to use temporarily to fill in data
    Event e;

    // Set ServiceID/NetworkID/TransportID since they remain the same per table
    e.ServiceID                  = head->table_id_ext;
    e.TransportID                = buffer[0] << 8 | buffer[1];
    e.NetworkID                  = buffer[2]  << 8 | buffer[3];

#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
    fprintf(stdout,"EIT_DEBUG: sid:%d nid:%04X tid:%04X lseg:%02X ltab:%02X tab:%02X sec:%02X lsec: %02X size:%d\n",
           e.ServiceID,
           e.NetworkID,
           e.TransportID,
           last_segment_number,
           last_table_id,
           head->table_id,
           head->section_number,
           head->section_last,
           size);
}
#endif

   // Loop through table (last 4 bytes are CRC)
   while (pos < (size-4))
   {

       e.EventID = buffer[pos] << 8 | buffer[pos+1];
       e.StartTime = ConvertDVBDate(&buffer[pos+2]);
       e.EndTime = e.StartTime.addSecs(
                       (bcdtoint(buffer[pos+7] & 0xFF) * 3600) +
                       (bcdtoint(buffer[pos+8] & 0xFF) * 60) +
                       (bcdtoint(buffer[pos+9] & 0xFF)) ) ;

#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
       fprintf(stdout,"EIT_EVENT: %d EventID: %d   Time: %s - %s\n",e.ServiceID,e.EventID,e.StartTime.toString(QString("yyyyMMddhhmm")).ascii(),e.EndTime.toString(QString("yyyyMMddhhmm")).ascii());
}
#endif

        // variables to store info about "best descriptor" 4D & 4E
        // (used to pick out the one with the preferred language 
        // in case there are more than one)
        int bd4D_prio = -1;
        uint8_t *bd4D_data = NULL;
        QString bd4D_lang;
        int bd4E_prio = -1;
        uint8_t *bd4E_data = NULL;
        QString bd4E_lang;

        // Parse descriptors
        descriptor_length = ((buffer[pos+10] & 0x0F) << 8) | buffer[pos+11];
        pos+=12;
        des_pos = pos;
        while (des_pos < (pos + descriptor_length)) 
        { 
            switch (buffer[des_pos]) 
            {
                case 0x4D:
                    {
                        QString lang = QString::fromLatin1((const char*) &buffer[des_pos + 2], 3);
                        int prio = LanguagePriority[lang];

#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
       fprintf(stdout,"EIT_EVENT: 4D descriptor, lang %s, prio %i\n", lang.ascii(), prio);
}
#endif

                        if ((prio > 0 && prio < bd4D_prio) || bd4D_prio == -1)
                        {
                            // this descriptor is better than what we have
                            // => store a reference to this one
                            bd4D_lang = lang;
                            bd4D_prio = prio;
                            bd4D_data = &buffer[des_pos];
                        }
                    }
                    break;

                case 0x4E:
                    {
                        QString lang = QString::fromLatin1((const char*) &buffer[des_pos + 3], 3);
                        int prio = LanguagePriority[lang];

#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
       fprintf(stdout,"EIT_EVENT: 4E descriptor, lang %s, prio %i\n", lang.ascii(), prio);
}
#endif

                        if ((prio > 0 && prio < bd4E_prio) || bd4E_prio == -1)
                        {
                            // this descriptor is better than what we have
                            // => store a reference to this one
                            bd4E_lang = lang;
                            bd4E_prio = prio;
                            bd4E_data = &buffer[des_pos];
                        }
                    }
                    break;

                case 0x50:
                    ProcessComponentDescriptor(&buffer[des_pos], buffer[des_pos+1]+2,e);
                    break;

                case 0x54:
                    e.ContentDescription =
                            ProcessContentDescriptor(&buffer[des_pos],buffer[des_pos+1]+2);
                    break;

                default:            
                    ProcessUnknownDescriptor(&buffer[des_pos],buffer[des_pos+1]+2);
                    break;
            }
            des_pos += (buffer[des_pos+1]+2);

            if (des_pos > size)
                return;
        }


        // Resolve data for "best" 4D & 4E descriptors
        if (bd4D_data != NULL)
        {
#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
        fprintf(stdout, "EIT_EVENT: using 4D data for language='%s'\n", bd4D_lang.ascii());
}
#endif
            e.LanguageCode = bd4D_lang;
            ProcessShortEventDescriptor(bd4D_data, bd4D_data[1] + 2, e);
        }
        if (bd4E_data != NULL)
        {
#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
        fprintf(stdout, "EIT_EVENT: using 4E data for language='%s'\n", bd4E_lang.ascii());
}
#endif
            e.LanguageCode = bd4E_lang;
            ProcessExtendedEventDescriptor(bd4E_data, bd4E_data[1] + 2, e);
        }


        EITFixUp(e);

#ifdef EIT_DEBUG_SID
if (e.ServiceID == EIT_DEBUG_SID) {
        fprintf(stdout, "EIT_EVENT: LanguageCode='%s' Event_Name='%s' Description='%s'\n", e.LanguageCode.ascii(), e.Event_Name.ascii(), e.Description.ascii());
}
#endif

        ((EventHandler*) Table[EVENTS])->Events[head->table_id_ext][e.EventID] = e;
        e.clearEventValues();
        pos += descriptor_length;
    }
}

/*------------------------------------------------------------------------
 *   COMMON DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/
// Descriptor 0x09 - Conditional Access Descriptor
CAPMTObject SIParser::ParseDescriptorCA(uint8_t* buffer, int size)
{
    (void) size;
    CAPMTObject retval;

    retval.CASystemID = buffer[2] << 8 | buffer[3];
    retval.PID = (buffer[4] & 0x1F) << 8 | buffer[5];
    retval.Data_Length = buffer[1] - 4;
    if (retval.Data_Length > 0)
    {
        memcpy(retval.Data, &buffer[6], retval.Data_Length);
    }

    return retval;
}

/*------------------------------------------------------------------------
 *   DVB DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/
// Descriptor 0x40 - NetworkName
void SIParser::ParseDescriptorNetworkName(uint8_t* buffer, int size, NetworkObject &n)
{
    (void) size;

    n.NetworkName = DecodeText(buffer + 2, buffer[1]);
}

// Descriptor 0x4A - Linkage - NIT
void SIParser::ParseDescriptorLinkage(uint8_t* buffer,int size,NetworkObject &n)
{
    (void) size;

    n.LinkageTransportID = buffer[2] << 8 | buffer[3];
    n.LinkageNetworkID = buffer[4] << 8 | buffer[5];
    n.LinkageServiceID = buffer[6] << 8 | buffer[7];
    n.LinkageType = buffer[8];
    n.LinkagePresent = 1;

    //The following was found to break EIT guide for 
    // Kristian Kalweit <kalweit@exorpro.de>
    if (n.LinkageType == 4)
    {
        PrivateTypes.GuideOnSingleTransport = true;
        PrivateTypes.GuideTransportID = n.LinkageTransportID;
    }
}

// Descriptor 0x62 - Frequency List - NIT
void SIParser::ParseDescriptorFrequencyList(uint8_t* buffer,int size, TransportObject& t)
{
    int i = 2;
    uint8_t coding = buffer[i++] & 0x3;
    unsigned frequency;
    QString FrequencyTemp;
    for (; i < size ; i+=4)
    {
         switch (coding)
         {
         case 0x3:  //DVB-T
             frequency*=10;
             frequency = (((buffer[i] << 24) | (buffer[i+1] << 16) | (buffer[i+2] << 8 ) | buffer[i+3]))*10;
             break;
         default:
              FrequencyTemp = QString("%1%2%3%4%5%6%7%800")
             .arg((buffer[i] & 0xF0) >> 4)
             .arg( buffer[i] & 0x0F)
             .arg((buffer[i+1] & 0xF0) >> 4)
             .arg( buffer[i+1] & 0x0F)
             .arg((buffer[i+2] & 0xF0) >> 4)
             .arg( buffer[i+2] & 0x0F)
             .arg((buffer[i+3] & 0xF0) >> 4)
             .arg( buffer[i+3] & 0x0F);
             frequency=FrequencyTemp.toInt();
         }
         t.frequencies+=frequency;
    }
}

//Descriptor 0x83 UK specific channel list
void SIParser::ParseDescriptorUKChannelList(uint8_t* buffer,int size, QMap_uint16_t& numbers)
{
    int i = 2;

    for (; i < size ; i+=4)
    {

        uint16_t service_id = (buffer[i]<<8)|(buffer[i+1]&0xff);
        uint16_t channel_num = (buffer[i+2]&0x03<<8)|(buffer[i+3]&0xff);

        numbers[service_id] = channel_num;
    }
}

// Desctiptor 0x48 - Service - SDT
void SIParser::ParseDescriptorService(uint8_t* buffer, int size, SDTObject& s)
{
    (void) size;
    uint8_t tempType = buffer[2];

    if (PrivateTypes.TVServiceTypes.contains(tempType))
        s.ServiceType = PrivateTypes.TVServiceTypes[tempType];
    else
        s.ServiceType = tempType;

    buffer += 3;
    s.ProviderName = DecodeText(buffer + 1, buffer[0]);
    buffer += buffer[0] + 1;
    s.ServiceName = DecodeText(buffer + 1, buffer[0]);
}

// Descriptor 0x5A - DVB-T Transport - NIT
TransportObject SIParser::ParseDescriptorTerrestrialDeliverySystem(uint8_t* buffer, int size)
{
    (void) size;

    TransportObject retval;

    retval.Type = QString("DVB-T");

    retval.Frequency = ((buffer[2] << 24) | (buffer[3] << 16) | (buffer[4] << 8 ) | buffer[5]) * 10;

    // Bandwidth
    switch ((buffer[6] & 0xE0) >> 5) {
       case 0:
               retval.Bandwidth = "8";
               break;
       case 1:
               retval.Bandwidth = "7";
               break;
       case 2:
               retval.Bandwidth = "6";
               break;
       default:
               retval.Bandwidth = "auto";
               break;
    }

    // Consetellation
    switch ((buffer[7] & 0xC0) >> 6) {
       case 0:
               retval.Constellation = "qpsk";
               break;
       case 1:
               retval.Constellation = "qam_16";
               break;
       case 2:
               retval.Constellation = "qam_64";
               break;
       default:
               retval.Constellation = "auto";
               break;
    }

    // Heiarchy
    switch ((buffer[7] & 0x38) >> 3) {
       case 0:
               retval.Hiearchy = "n";
               break;
       case 1:
               retval.Hiearchy = "1";
               break;
       case 2:
               retval.Hiearchy = "2";
               break;
       case 3:
               retval.Hiearchy = "4";
               break;
       default:
               retval.Hiearchy = "a";
               break;
    }

    // CoderateHP
    switch (buffer[7] & 0x03) 
    {
    case 0:
        retval.CodeRateHP = "1/2";
        break;
    case 1:
        retval.CodeRateHP = "2/3";
        break;
    case 2:
        retval.CodeRateHP = "3/4";
        break;
    case 3:
        retval.CodeRateHP = "5/6";
        break;
    case 4:
        retval.CodeRateHP = "7/8";
        break;
    default:
        retval.CodeRateHP = "auto";
    }

    // CoderateLP
    switch ((buffer[8] & 0xC0) >> 6) 
    {
    case 0:
        retval.CodeRateLP = "1/2";
        break;
    case 1:
        retval.CodeRateLP = "2/3";
        break;
    case 2:
        retval.CodeRateLP = "3/4";
        break;
    case 3:
        retval.CodeRateLP = "5/6";
        break;
    case 4:
        retval.CodeRateLP = "7/8";
        break;
    default:
        retval.CodeRateLP = "auto";
    }    
    
    //Guard Interval
    switch ((buffer[8] & 0x18) >> 3) 
    {
    case 0:
        retval.GuardInterval = "1/32";
        break;
    case 1:
        retval.GuardInterval = "1/16";
        break;
    case 2:
        retval.GuardInterval = "1/8";
        break;
    case 3:
        retval.GuardInterval = "1/4";
        break;
    } 
       
    //TransmissionMode
    switch ((buffer[8] & 0x06) >> 1) 
    {
    case 0:
        retval.TransmissionMode = "2";
        break;
    case 1:
        retval.TransmissionMode = "8";
        break;
    default:
        retval.TransmissionMode = "auto";
    }    
    return retval;
}

// Desctiptor 0x43 - Satellite Delivery System - NIT
TransportObject SIParser::ParseDescriptorSatelliteDeliverySystem(uint8_t* buffer, int size)
{

    (void) size;
    TransportObject retval;

    retval.Type = QString("DVB-S");

    QString FrequencyTemp = QString("%1%2%3%4%5%6%7%80")
             .arg((buffer[2] & 0xF0) >> 4)
             .arg( buffer[2] & 0x0F)
             .arg((buffer[3] & 0xF0) >> 4)
             .arg( buffer[3] & 0x0F)
             .arg((buffer[4] & 0xF0) >> 4)
             .arg( buffer[4] & 0x0F)
             .arg((buffer[5] & 0xF0) >> 4)
             .arg( buffer[5] & 0x0F);

    // TODO: Use real BCD conversion on Frequency
    retval.Frequency=FrequencyTemp.toInt();

    retval.OrbitalLocation=QString("%1%2.%3").arg(buffer[6],0,16).arg((buffer[7]&0xF0) >> 4).arg(buffer[7] & 0x0F);

    // This isn't reported correctly by some carriers
    switch ((buffer[8] & 0x80) >> 7) {
       case 0:
               retval.OrbitalLocation += " West";
               break;
       case 1:
               retval.OrbitalLocation += " East";
               break;
    }

    switch ((buffer[8] & 0x60) >> 5) {
       case 0:
               retval.Polarity = "h";
               break;
       case 1:
               retval.Polarity = "v";
               break;
       case 2:
               retval.Polarity = "l";
               break;
       case 3:
               retval.Polarity = "r";
               break;
    }

    switch (buffer[8] & 0x1F) {
    case 0:  // Some SAT Providers use this for QPSK for some reason
             // Bell ExpressVu is an example
    case 1:
            retval.Modulation = "qpsk";
        break;
    case 2:
            retval.Modulation = "qpsk_8";
        break;
    case 3:
            retval.Modulation = "qam_16";
        break;
    default:
        retval.Modulation = "auto";
    }

    QString SymbolRateTemp=QString("%1%2%3%4%5%6%700")
             .arg((buffer[9] & 0xF0) >> 4)
             .arg( buffer[9] & 0x0F)
             .arg((buffer[10] & 0xF0) >> 4)
             .arg( buffer[10] & 0x0F)
             .arg((buffer[11] & 0xF0) >> 4)
             .arg( buffer[11] & 0x0F)
             .arg((buffer[12] & 0xF0) >> 4);

    retval.SymbolRate = SymbolRateTemp.toInt();

    switch (buffer[12] & 0x0F)
    {
    case 1:
        retval.FEC_Inner = "1/2";
        break;
    case 2:
        retval.FEC_Inner = "2/3";
        break;
    case 3:
        retval.FEC_Inner = "3/4";
        break;
    case 4:
        retval.FEC_Inner = "5/6";
        break;
    case 5:
        retval.FEC_Inner = "7/8";
        break;
    case 6:
        retval.FEC_Inner = "8/9";
        break;
    case 0x0F:
        retval.FEC_Inner = "none";
        break;
    default:
        retval.FEC_Inner = "auto";
    }

    return retval;
}

// Descriptor 0x44 - Cable Delivery System - NIT
TransportObject SIParser::ParseDescriptorCableDeliverySystem(uint8_t* buffer, int size)
{
     (void) size;
     TransportObject retval;

     retval.Type = QString("DVB-C");

     QString FrequencyTemp = QString("%1%2%3%4%5%6%7%800")
             .arg((buffer[2] & 0xF0) >> 4)
             .arg( buffer[2] & 0x0F)
             .arg((buffer[3] & 0xF0) >> 4)
             .arg( buffer[3] & 0x0F)
             .arg((buffer[4] & 0xF0) >> 4)
             .arg( buffer[4] & 0x0F)
             .arg((buffer[5] & 0xF0) >> 4)
             .arg( buffer[5] & 0x0F);

     // TODO: Use real BCD conversion on Frequency
     retval.Frequency=FrequencyTemp.toInt();


     switch (buffer[7] & 0x0F) {
         case 1:
                 retval.FEC_Outer = "None";
                 break;
         case 2:
                 retval.FEC_Outer = "RS(204/188)";
                 break;
         default:
                 retval.FEC_Outer = "unknown";
                 break;
     }

     switch (buffer[8]) {
         case 1:
                 retval.Modulation = "qam_16";
                 break;
         case 2:
                 retval.Modulation = "qam_32";
                 break;
         case 3:
                 retval.Modulation = "qam_64";
                 break;
         case 4:
                 retval.Modulation = "qam_128";
                 break;
         case 5:
                 retval.Modulation = "qam_256";
                 break;
         default:
                 retval.Modulation = "auto";
                 break;
     }

     QString SymbolRateTemp=QString("%1%2%3%4%5%6%700")
              .arg((buffer[9] & 0xF0) >> 4)
              .arg( buffer[9] & 0x0F)
              .arg((buffer[10] & 0xF0) >> 4)
              .arg( buffer[10] & 0x0F)
              .arg((buffer[11] & 0xF0) >> 4)
              .arg( buffer[11] & 0x0F)
              .arg((buffer[12] & 0xF0) >> 4);

     retval.SymbolRate = SymbolRateTemp.toInt();

     switch (buffer[12] & 0x0F) {
         case 1:
                 retval.FEC_Inner = "1/2";
                 break;
         case 2:
                 retval.FEC_Inner = "2/3";
                 break;
         case 3:
                 retval.FEC_Inner = "3/4";
                 break;
         case 4:
                 retval.FEC_Inner = "5/6";
                 break;
         case 5:
                 retval.FEC_Inner = "7/8";
                 break;
         case 6:
                 retval.FEC_Inner = "8/9";
                 break;
         case 0x0F:
                 retval.FEC_Inner = "none";
                 break;
         default:
                 retval.FEC_Inner = "auto";
                 break;
     }

     return retval;
}

/*
 *  DVB Descriptor 0x54 - Process Content Descriptor - EIT
 */
QString SIParser::ProcessContentDescriptor(uint8_t *buf,int size)
{
// TODO: Add all types, possibly just lookup from a big array that is an include file?
    (void) size;
    uint8_t content = buf[2];
    if (content)
        return m_mapCategories[content];
    else
        return QString();
}

/*
 *  DVB Descriptor 0x4D - Short Event Descriptor - EIT
 */

void SIParser::ProcessShortEventDescriptor(uint8_t *buf,int size,Event& e)
{
// TODO: Test lower loop

    (void) size;

    int event_name_len = buf[5] & 0xFF;
    int text_char_len = buf[event_name_len + 6];;

    e.Event_Name = DecodeText(buf + 6, event_name_len);
    e.Event_Subtitle = DecodeText(buf + event_name_len + 7, text_char_len);

    if(e.Event_Subtitle == e.Event_Name)
        e.Event_Subtitle= "";
}


/*
 *  DVB Descriptor 0x4E - Extended Event - EIT
 */
void SIParser::ProcessExtendedEventDescriptor(uint8_t *buf,int size,Event &e)
{

   (void) size;

// TODO: Nothing, Complete

    if (buf[6] != 0)
      return;

    int text_length = buf[7] & 0xFF;
    e.Description = DecodeText(buf + 8, text_length);

}

QString SIParser::ParseDescriptorLanguage(uint8_t* buffer, int size)
{
    (void) size;
    return QString::fromLatin1((const char*) buffer + 2, 3);
}

void SIParser::ProcessComponentDescriptor(uint8_t *buf,int size,Event& e)
{
   (void)size;
   switch (buf[2] & 0x0f)
   {
   case 0x1: //Video
       if ((buf[3]>=0x9) && (buf[3]<=0x10))
           e.HDTV = true;
       break;
   case 0x2: //Audio
       if ((buf[3]=0x3) || (buf[3]=0x5))
           e.Stereo = true;
       break;
   case 0x3: //Teletext/Subtitles
       switch (buf[3])
       {
       case 0x1:
       case 0x3:
       case 0x10 ... 0x13:
       case 0x20 ... 0x23:
            e.SubTitled = true;
       }
       break;
   }
}

void SIParser::ParseDescriptorTeletext(uint8_t* buffer, int size)
{
    SIPARSER(QString("Teletext Descriptor"));

    buffer += 2;

    while (size >= 5)
    {
        QString language = QString::fromLatin1((const char*) buffer, 3);
        uint8_t teletext_type = buffer[3] >> 3;
        uint8_t teletext_magazine_number = buffer[3] & 0x07;
        uint8_t teletext_page_number = buffer[4];

        SIPARSER(QString("  lang: %1, type: %2, mag: %3, page: %4")
                    .arg(language)
                    .arg(teletext_type)
                    .arg(teletext_magazine_number)
                    .arg(teletext_page_number)
                );

        buffer += 5;
        size -= 5;
    }
}

void SIParser::ParseDescriptorSubtitling(uint8_t* buffer, int size)
{
    SIPARSER(QString("Subtitling Descriptor"));

    buffer += 2;

    while (size >= 8)
    {
        QString language = QString::fromLatin1((const char*) buffer, 3);
        uint8_t subtitling_type = buffer[3];
        uint16_t composition_page_id = (buffer[4] << 8) | buffer[5];
        uint16_t ancillary_page_id = (buffer[6] << 8) | buffer[7];

        SIPARSER(QString("  lang: %1, type: %2, comp: %3, anc: %4")
                    .arg(language)
                    .arg(subtitling_type)
                    .arg(composition_page_id)
                    .arg(ancillary_page_id)
                );

        buffer += 8;
        size -= 8;
    }
}

/*------------------------------------------------------------------------
 *   ATSC HELPER PARSERS
 *------------------------------------------------------------------------*/

QDateTime SIParser::ConvertATSCDate(uint32_t offset)
{
// TODO: Clean this up its a mess right now to get it to localtime
    QDateTime ATSCEPOC = QDateTime(QDate(1980,1,6));
    // Get event time and add on GPS Second Difference
//    QDateTime UTCTime = ATSCEPOC.addSecs(offset - (((STTHandler*) Table[STT])->GPSOffset) );
    QDateTime UTCTime = ATSCEPOC.addSecs(offset - 13 );
    // Get UTC
    QDateTime UTCEPOC = QDateTime(QDate(1970,1,1));
    // Now sloppily convert it to localtime

    int timesecs = UTCEPOC.secsTo(UTCTime);
    QDateTime LocalTime;
    LocalTime.setTime_t(timesecs);
    return LocalTime;
}

/*
 * Global ATSC Multiple String Format Parser into QString(s)
 */
QString SIParser::ParseMSS(uint8_t* buffer, int size)
{
// TODO: Check size
//       Deal with multiple strings.
//       Handle hufmann encoded text

   (void) size;

   QString retval;

   // Language bytes 1,2,3 - Use DVB Language Function
   // Segments 4
   // Compression Type = 5
   // Mode = 6
   // Bytes = 7

    if (buffer[4] > 1)
        SIPARSER("MSS WITH MORE THAN 1 SEGMENT");

    switch (buffer[5])
    {
        case 0:
                 for (int z=0; z < buffer[7]; z++)
                     retval += QChar(buffer[8+z]);
                 break;

        case 1:
                 retval = HuffmanToQString(&buffer[8],buffer[7],ATSC_C5);
                 break;

        case 2:
                 retval = HuffmanToQString(&buffer[8],buffer[7],ATSC_C7);
                 break;

        default:
                 retval = QString("Unknown compression");
                 break;
    }
    return retval;

}

/*------------------------------------------------------------------------
 *   ATSC TABLE PARSERS
 *------------------------------------------------------------------------*/

/*
 *  ATSC Table 0xC7 - Master Guide Table - PID 0x1FFB
 */
void SIParser::ParseMGT(tablehead_t* head, uint8_t* buffer, int size)
{

    (void) size;

    if (Table[MGT]->AddSection(head,0,0))
        return;

    uint16_t pos = 3;
    uint16_t tables_defined = buffer[1] << 8 | buffer[2];;

    for (int x = 0 ; x < tables_defined ; x++)
    {
        uint16_t table_type = buffer[pos] << 8 | buffer[pos+1];
        uint16_t table_type_pid = (buffer[pos+2] & 0x1F) << 8 | buffer[pos+3];
        uint32_t size = (buffer[pos+5] << 24) |
                        (buffer[pos+6] << 16) |
                        (buffer[pos+7] << 8) |
                        (buffer[pos+8]);

        uint16_t descriptors_length = (buffer[pos+9] & 0x0F) << 8 | buffer[pos+10];

        switch (table_type)
        {
            case 0x00 ... 0x03:
                               TableSourcePIDs.ServicesPID = table_type_pid;
                               TableSourcePIDs.ServicesMask = 0xFF;
                               if (table_type == 0x02)
                               {
                                    TableSourcePIDs.ServicesTable = 0xC9;
                                    SIPARSER("CVCT Present on this Transport");
                               }
                               if (table_type == 0x00)
                               {
                                   TableSourcePIDs.ServicesTable = 0xC8;
                                   SIPARSER("TVCT Present on this Transport");
                               }
                               break;
            case 0x04:
                               TableSourcePIDs.ChannelETT = table_type_pid;
                               SIPARSER(QString("Channel ETT Present on PID %1 (%2)")
                                        .arg(table_type_pid,4,16).arg(size));
                               break;

            case 0x100 ... 0x17F:
                               SIPARSER(QString("EIT-%1 Present on PID %2")
                                       .arg(table_type - 0x100)
                                       .arg(table_type_pid,4,16));

                               Table[EVENTS]->AddPid(table_type_pid,0xCB,0xFF,table_type - 0x100);
                               break;

            case 0x200 ... 0x27F:
                               SIPARSER(QString("ETT-%1 Present on PID %2")
                                       .arg(table_type - 0x200)
                                       .arg(table_type_pid,4,16));
                               Table[EVENTS]->AddPid(table_type_pid,0xCC,0xFF,table_type - 0x200);
                               break;

            default:
                               SIPARSER(QString("Unknown Table %1 in MGT on PID %2")
                                       .arg(table_type,4,16)
                                       .arg(table_type_pid,4,16));
                               break;
        }                    
        pos += 11;
        pos += descriptors_length;

    }
}

/*
 *  ATSC Table 0xC8/0xC9 - Terrestrial/Cable Virtual Channel Table - PID 0x1FFB
 */
void SIParser::ParseVCT(tablehead_t* head, uint8_t* buffer, int size)
{

// Prerequisites: MGT, and CHANNEL_ETT

    emit TableLoaded();

    if (Table[SERVICES]->AddSection(head,0,0))
        return;

    uint8_t num_channels_in_section = buffer[1];
    uint16_t pos = 2, lentotal = 0, len = 0;

    SDTObject s;

    for (int x = 0; x < num_channels_in_section ; x++)
    {
        for (int y = 0; y < 14 ; y+=2)
        {
            uint16_t temp = buffer[pos+y] << 8 | buffer[pos+y+1];
            s.ServiceName += QChar(temp);
        }

        uint16_t major_channel_number = ((buffer[pos+14] & 0x0F) >> 2) | ((buffer[pos+15] & 0xFC) >> 2);
        uint16_t minor_channel_number = (buffer[pos+15] & 0x03) << 2 | (buffer[pos+16]);
        s.ChanNum = (major_channel_number * 10) + minor_channel_number;

        s.TransportID = buffer[pos+22] << 8 | buffer[pos+23];

        if (PrivateTypesLoaded == false)
            LoadPrivateTypes(s.TransportID);

//        uint8_t ETM_Location = (buffer[pos+26] & 0xC0) >> 6;
        s.CAStatus = (buffer[pos+26] & 0x20) >> 5;
        s.ServiceID = buffer[pos+24] << 8 | buffer[pos+25];

        s.ATSCSourceID = buffer[pos+28] << 8 | buffer[pos+29];
#ifdef USING_DVB_EIT
        Table[EVENTS]->RequestEmit(s.ATSCSourceID);
#endif

        s.Version = head->version;
        s.ServiceType = 1;
        s.EITPresent = 1;

        SIPARSER(QString("Found Channel %1-%2 - %3 CAStatus=%4").arg(major_channel_number)
                 .arg(minor_channel_number).arg(s.ServiceName).arg(s.CAStatus));

        uint16_t descriptors_length = (buffer[pos+30] & 0x02) << 8 | buffer[pos+31];

        lentotal = 0;

        while ((descriptors_length) > (lentotal))
        {
            switch(buffer[pos + 32 + lentotal])
            {
/*                case 0xA0:   s.ServiceName = ParseATSCExtendedChannelName
                                            (&buffer[pos + 32 + lentotal],buffer[pos + 33 + lentotal]);
                             break;
*/
                default:
                             ProcessUnknownDescriptor(&buffer[pos + 32 + lentotal],buffer[pos + 33 + lentotal]);
                             break;
            }
            len = buffer[pos + 33 + lentotal];
            lentotal += (len + 2);
        }

        pos += (32 + descriptors_length);

        /* Do not add in minor_channel 0 since its the analog version */
        if (minor_channel_number != 0)
            ((ServiceHandler*) Table[SERVICES])->Services[0][s.ServiceID] = s;
        s.Reset();

    }

// TODO REMOVE THIS WHEN SERVIVES SET
    Table[EVENTS]->DependencyMet(SERVICES);

    emit FindServicesComplete();

    (void)head;
    (void)buffer;
    (void)size;
}

/*
 *  ATSC Table 0xCA - Rating Region Table - PID 0x1FFB
 */
void SIParser::ParseRRT(tablehead_t* head, uint8_t* buffer, int size)
{
// TODO: Decide what to do with this.  There currently is no field in Myth for a rating

    (void)head;
    (void)buffer;
    (void)size;

    return;

    uint16_t pos = 0;

    QString temp = ParseMSS(&buffer[2],buffer[1]);

    pos += buffer[1] + 2;
    uint8_t dimensions = buffer[pos++];

    for (int x = 0 ; x < dimensions ; x++)
    {
        QString Name = ParseMSS(&buffer[pos + 1],buffer[pos]);
        /* Skip past Dimesion Name */
        pos += buffer[pos] + 1;

        /* Skip past Values Defined */
        uint8_t values = buffer[pos] & 0x0F;
        pos++;

        for (int y = 0 ; y < values ; y++)
        {
            QString Value_Text = ParseMSS(&buffer[pos + 1],buffer[pos]);
            pos += buffer[pos] + 1;

            QString Rating_Value_Text = ParseMSS(&buffer[pos + 1],buffer[pos]);
            pos += buffer[pos] + 1;

        }

    }
}

/*
 *  ATSC Table 0xCB - Event Information Table - PID Varies
 */
void SIParser::ParseATSCEIT(tablehead_t* head, uint8_t* buffer, int size, uint16_t pid)
{

    (void) size;

    if (Table[EVENTS]->AddSection(head,head->table_id_ext,pid))
        return;

    Event e;

    uint8_t num_events = buffer[1];
    uint16_t pos = 2, lentotal = 0, len = 0;

    for (int z=0;z < num_events; z++)
    {
        e.SourcePID = pid;
        uint16_t event_id = ((buffer[pos] & 0x3F) << 8) | buffer[pos+1];
        uint32_t start_time_offset = buffer[pos+2] << 24 |
                              buffer[pos+3] << 16 |
                              buffer[pos+4] << 8  |
                              buffer[pos+5];

        e.StartTime = ConvertATSCDate(start_time_offset);
        e.ETM_Location  = (buffer[pos+6] & 0x30 ) >> 4;

        uint32_t length_in_seconds = (buffer[pos+6] & 0x0F) << 16 |
                                     buffer[pos+7] << 8 |
                                     buffer[pos+8];
        e.EndTime = e.StartTime.addSecs(length_in_seconds);
        e.ServiceID = head->table_id_ext;
        e.ATSC = true;

        uint8_t title_length = buffer[pos+9];
        e.Event_Name = ParseMSS(&buffer[pos+10],title_length);

#ifdef EIT_DEBUG_SID
        fprintf(stdout,"Title [%02X][%04X] = %s  %s - %s\n",head->table_id_ext,event_id,e.Event_Name.ascii(),e.StartTime.toString(Qt::TextDate).ascii(),
                                           e.EndTime.toString(Qt::TextDate).ascii());
#endif

        pos += (title_length + 10);

        uint16_t descriptors_length = (buffer[pos] & 0x0F) << 8 | buffer[pos+1];

        lentotal = 0;
        while ((descriptors_length) > (lentotal))
        {
            switch(buffer[pos + 2 + lentotal])
            {
            case 0x86:    //TODO: ATSC Caption Descriptor
                break;
            case 0x87:   // Content Advisory Decriptor
                ParseDescriptorATSCContentAdvisory(&buffer[pos + 2 + lentotal],
                            buffer[pos + 3 + lentotal]);
            break;
            default:
                ProcessUnknownDescriptor(&buffer[pos + 2 + lentotal],buffer[pos + 3 + lentotal]);
                break;
            }
            len = buffer[pos + 3 + lentotal];
            lentotal += (len + 2);
        }

        pos += (descriptors_length + 2);
  
        EITFixUp(e);

        ((EventHandler*) Table[EVENTS])->Events[head->table_id_ext][event_id] = e;
        e.Reset();
    }
}

/*
 *  ATSC Table 0xCC - Extended Text Table - PID Varies
 */
void SIParser::ParseETT(tablehead_t* head, uint8_t* buffer, int size, uint16_t pid)
{

    (void) head;
    (void) size;
    (void) pid;

    uint16_t source_id = buffer[1] << 8 | buffer[2];
    uint16_t etm_id = buffer[3] << 8 | buffer[4];

    if ((etm_id & 0x03) == 2)
    {
          if ( ((EventHandler*) Table[EVENTS])->Events[source_id].contains(etm_id >> 2) )
          {
              if ( ((EventHandler*) Table[EVENTS])->Events[source_id][etm_id >>2].ETM_Location != 0 )
              {
                  QString temp = ParseMSS(&buffer[5],size-5);
                  ((EventHandler*) Table[EVENTS])->Events[source_id][etm_id >> 2].Description = temp;

                  EITFixUp(((EventHandler*) Table[EVENTS])->Events[source_id][etm_id >> 2]);

                  ((EventHandler*) Table[EVENTS])->Events[source_id][etm_id >> 2].ETM_Location = 0;
             
             }
        }
    }
}

/*
 *  ATSC Table 0xCD - System Time Table - PID 0x1FFB
 */
void SIParser::ParseSTT(tablehead_t* head, uint8_t* buffer, int size)
{
    (void)head;
    (void)size;

    if (Table[STT]->AddSection(head,0,0))
        return;

    ((STTHandler*) Table[STT])->GPSOffset = buffer[5];

    SIPARSER(QString("GPS Time Offset = %1 Seconds").arg(buffer[5]));
}


/*
 *  ATSC Table 0xD3 - Directed Channel Change Table - PID 0x1FFB
 */
void SIParser::ParseDCCT(tablehead_t* head, uint8_t* buffer, int size)
{
    (void)head;
    (void)buffer;
    (void)size;

}

/*
 *  ATSC Table 0xD4 - Directed Channel Change Selection Code Table - PID 0x1FFB
 */
void SIParser::ParseDCCSCT(tablehead_t* head, uint8_t* buffer, int size)
{
    (void)head;
    (void)buffer;
    (void)size;
}

/*------------------------------------------------------------------------
 *   ATSC DESCRIPTOR PARSERS
 *------------------------------------------------------------------------*/

QString SIParser::ParseATSCExtendedChannelName(uint8_t* buffer, int size)
{

    return ParseMSS(&buffer[2],size);

}

// TODO: Use this descriptor parser
void SIParser::ParseDescriptorATSCContentAdvisory(uint8_t* buffer, int size)
{

    (void) buffer;
    (void) size;

    return;

    uint8_t pos = 3;
    QString temp = "";

    for (int x = 0 ; x < (buffer[2] & 0x3F) ; x++)
    {
        uint8_t dimensions = buffer[pos+1];
        pos += 2;
        for (int y = 0 ; y < dimensions ; y++)
            pos += 2;
        if (buffer[pos] > 0)
            temp = ParseMSS(&buffer[pos+1],buffer[pos]);
        pos += buffer[pos] + 1;
    }

}

// TO BE WRITTEN

/*------------------------------------------------------------------------
 *   TABLE VERIFIERS - These need to be re-written or placed into the Parse
 *                     functions
 *------------------------------------------------------------------------*/

bool SIParser::verifyPAT(tablehead_t* head, uint8_t *data, int len)
{
    (void) data;

    // Verify the length the table suggests is really the length
    if (head->section_length != (len + 5))
        return false;

    // Make sure the length a multiple of four
    if (len %4 != 0)
        return false;

    return true;
}

bool SIParser::verifySDT(uint8_t *buf, int len)
{
    if (len < 10)
        return false;

    if (WORD(buf[1] & 0x0f, buf[2]) >= 1021)
        return false;
    if (len >= 1024)
        return false;
    if (!(buf[5] & 0x01))
        return false;

    int pos = 11;
     while(pos + 5 < len)
    {
    uint16_t dlen = WORD(buf[pos+3] & 0xf, buf[pos+4]);

    pos += 5;
    while (dlen > 0)
    {
      dlen -= 2 + buf[pos+1];
      pos += 2 + buf[pos+1];
      if (pos > len)
        return false;
    }

    if (dlen != 0)
      return false;
  }

  pos += 4;
  if (len != pos)
    return false;

  return true;
 }

bool SIParser::verifyNIT(uint8_t *buf, int len)
 {
  if (len < 10)
    return false;

  uint16_t nd_len = WORD(buf[8] & 0xf, buf[9]);

  int pos = 10;
  while (pos < nd_len + 10)
    pos += 2 + buf[pos+1];

  if (pos != nd_len + 10)
    return false;

  pos += 2;
  while (pos + 6 < len)
  {
    uint16_t td_len = WORD(buf[pos+4] & 0x0f, buf[pos+5]);
    pos += 6;
    while (td_len > 0)
    {
      td_len -= 2 + buf[pos+1];
      pos += 2 + buf[pos+1];
    }
    if (td_len != 0)
      return false;
  }

  pos += 4;
  if (pos != len)
    return false;

  return true;
 }

bool SIParser::verifyPMT(uint8_t *data, int len)
{
  if (len < 4)
    return false;

  int pinfo_len = (data[2] & 0x0f) << 8 | data[3];
  int pos = 4;

  while (pos < pinfo_len + 4)
    pos += 2 + data[pos+1];

  if (pos != pinfo_len + 4)
    return false;

  while (pos + 5 < len)
  {
    int es_len = (data[pos+3] & 0x0f) << 8 | data[pos+4];
    pos += 5;
    while (es_len > 0)
    {
      es_len -= 2 + data[pos+1];
      pos += data[pos+1];
      pos += 2;
    }
    if (es_len != 0)
      return false;
  }

  pos += 4; /* crc */

  if (pos != len)
    return false;

  return true;
 }

/*------------------------------------------------------------------------
 * Huffman Text Decompressors - 1 and 2 level routines. Tables defined in
 * atsc_huffman.h
 *------------------------------------------------------------------------*/

/* returns the root for character Input from table Table[] */
int SIParser::HuffmanGetRootNode(uint8_t Input, uint8_t Table[])
{
    if (Input > 127)
        return -1;
    else
        return (Table[Input * 2] << 8) | Table[(Input * 2) + 1];
}

/* Returns the bit number bit from string test[] */
bool SIParser::HuffmanGetBit(uint8_t test[], uint16_t bit)
{
    return (test[(bit - (bit % 8)) / 8] >> (7-(bit % 8))) & 0x01;
}

QString SIParser::HuffmanToQString(uint8_t test[], uint16_t size, uint8_t Table[])
{

    QString retval;

    int totalbits = size * 8;
    int bit = 0;
    int root = HuffmanGetRootNode(0,Table);
    int node = 0;
    bool thebit;
    unsigned char val;

    while (bit < totalbits)
    {
        thebit = HuffmanGetBit(test,bit);
        val = thebit? Table[root+(node*2)+1] : Table[root+(node*2)];

        if (val & 0x80)
        {
            /* Got a Null Character so return */
            if ((val & 0x7F) == 0)
            {
                return retval;
            }
            /* Escape character so next character is uncompressed */
            if ((val & 0x7F) == 27)
            {
                unsigned char val2 = 0;
                for (int i = 0 ; i < 7 ; i++)
                {
                    val2 |= (HuffmanGetBit(test,bit+i+2) << 6-i);

                }
                retval += QChar(val2);
                bit += 8;
                root = HuffmanGetRootNode(val2,Table);
            }
            /* Standard Character */
            else
            {
                root = HuffmanGetRootNode(val & 0x7F,Table);
                retval += QChar(val & 0x7F);
            }
            node = 0;
        }
        else
            node = val;
        bit++;
    }
    /* If you get here something went wrong so just return a blank string */
    return QString("");
}

int SIParser::Huffman2GetBit ( int bit_index, unsigned char *byteptr )
{
    int byte_offset;
    int bit_number;

    byte_offset = bit_index / 8;
    bit_number  = bit_index - ( byte_offset * 8 );

    if ( byteptr[ byte_offset ] & ( 1 << (7 - bit_number) ) )
        return 1;
    else
        return 0;
}

uint16_t SIParser::Huffman2GetBits ( int bit_index, int bit_count, unsigned char *byteptr )
{
    int i;
    unsigned int bits = 0;

    for ( i = 0 ; i < bit_count ; i++ )
        bits = ( bits << 1 ) | Huffman2GetBit( bit_index + i, byteptr );

    return bits;
}

int SIParser::Huffman2ToQString (unsigned char *compressed, int length, int table, QString &Decompressed)
{
    int            i;
    int            total_bits;
    int            current_bit = 0;
    int            count = 0;
    unsigned int   bits;
    int            table_size;
    struct huffman_table *ptrTable;

    // Determine which huffman table to use
    if ( table == 1 )
    {
        table_size = 128;
        ptrTable   = Table128; 
    }
    else
    {
        table_size = 255;
        ptrTable   = Table255; 
    }
    total_bits = length * 8;
    Decompressed = "";

    // walk thru all the bits in the byte array, finding each sequence in the
    // list and decoding it to a character.
    while ( current_bit < total_bits - 3 )
    {
        for ( i = 0; i < table_size; i++ )
        {
            bits = Huffman2GetBits( current_bit, ptrTable[i].number_of_bits, compressed );
            if ( bits == ptrTable[i].encoded_sequence ) 
            {
                if ((ptrTable[i].character < 128) && (ptrTable[i].character > 30))
                   Decompressed += QChar(ptrTable[i].character);
                current_bit += ptrTable[i].number_of_bits;
                break;
            }
        }
        // if we get here then the bit sequence was not found ... 
        // ...problem try to recover
        if ( i == table_size ) 
            current_bit += 1;
    }
    return count;
}

/* Huffman Text Decompression Routines used by some Nagra Providers */
void SIParser::ProcessDescriptorHuffmanEventInfo(unsigned char *buf, unsigned int len, Event& e)
{
    (void) len;

    QString decompressed;

    if ((buf[4] & 0xF8) == 0x80)
       Huffman2ToQString(buf+5, buf[1]-3 , 2, decompressed);
    else
       Huffman2ToQString(buf+4, buf[1]-2 , 2, decompressed);

    QStringList SplitValues = QStringList::split("}{",decompressed);

    uint8_t switchVal = 0;
    QString temp;

    for ( QStringList::Iterator it = SplitValues.begin(); it != SplitValues.end(); ++it ) 
    {
        (*it).replace( "{" , "" );
        (*it).replace( "}" , "" );
        switchVal = (*it).left(1).toInt();
        temp = (*it).mid(2);

        switch (switchVal)
        {
            case 1:     
                          /* Sub Title */
                          e.Event_Subtitle = temp;
                          break; 
            case 2:     
                          /* Category */
                          e.ContentDescription = temp;
                          break; 
            case 4:     
                          e.Year = temp;
                          break; 
            case 5:     
                          e.Description = temp;
                          break; 
            case 7:     
                          /* Audio */
                          break; 
            case 6:     
                          /* Subtitles */
                          break; 
            default:     
                          break; 

        }
    }

}

/* Used by some Nagra Systems for Huffman Copressed Guide */
QString SIParser::ProcessDescriptorHuffmanText(unsigned char *buf,unsigned int len)
{

    (void) len;

    QString decompressed;

    if ((buf[3] & 0xF8) == 0x80)
       Huffman2ToQString(buf+4, buf[1]-2, 2, decompressed);
    else
       Huffman2ToQString(buf+3, buf[1]-1, 2, decompressed);

    return decompressed;

}

QString SIParser::ProcessDescriptorHuffmanTextLarge(unsigned char *buf,unsigned int len)
{
    (void) len;

    QString decompressed;

    if ((buf[4] & 0xF8) == 0x80)
       Huffman2ToQString(buf+5, buf[1]-3 , 2, decompressed);
    else
       Huffman2ToQString(buf+4, buf[1]-2 , 2, decompressed);

    return decompressed;

}

/*------------------------------------------------------------------------
 * Event Fix Up Scripts - Turned on by entry in dtv_privatetype table
 *------------------------------------------------------------------------*/

void SIParser::EITFixUp(Event& event)
{
    if (event.Description == "" && event.Event_Subtitle != "") 
    {
         event.Description = event.Event_Subtitle;        
         event.Event_Subtitle="";        
    }

    switch (PrivateTypes.EITFixUp)
    {   
        case 1:    EITFixUpStyle1(event);
                   break;
        case 2:    EITFixUpStyle2(event);
                   break;
        case 3:    EITFixUpStyle3(event);
                   break;
        case 4:    EITFixUpStyle4(event);
                   break;
        default:
                   break; 
    }

}

void SIParser::EITFixUpStyle1(Event& event)
{

    // This function does some regexps on Guide data to get more powerful guide.
    // Use this for BellExpressVu

    // TODO: deal with events that don't have eventype at the begining?

    QString Temp = "";

    int8_t position;

    // A 0x0D character is present between the content and the subtitle if its present
    position = event.Description.find(0x0D);

    if (position != -1)
    {
       // Subtitle present in the title, so get it and adjust the description
       event.Event_Subtitle = event.Description.left(position);
       event.Description = event.Description.right(event.Description.length()-position-2);
    }

    // Take out the content description which is always next with a period after it
    position = event.Description.find(".");
    // Make sure they didn't leave it out and you come up with an odd category
    if (position < 10)
    {
       event.ContentDescription = event.Description.left(position);
       event.Description = event.Description.right(event.Description.length()-position-2);
    }
    else
    {
       event.ContentDescription = "Unknown";
    }

    // When a channel is off air the category is "-" so leave the category as blank
    if (event.ContentDescription == "-")
       event.ContentDescription = "OffAir";

    if (event.ContentDescription.length() > 10)
       event.ContentDescription = "Unknown";


    // See if a year is present as (xxxx)
    position = event.Description.find(QRegExp("[\\(]{1}[0-9]{4}[\\)]{1}"));
    if (position != -1 && event.ContentDescription != "")
    {
       Temp = "";
       // Parse out the year
       event.Year = event.Description.mid(position+1,4);
       // Get the actors if they exist
       if (position > 3)
       {
          Temp = event.Description.left(position-3);
          event.Actors = QStringList::split(QRegExp("\\set\\s|,"), Temp);
       }
       // Remove the year and actors from the description
       event.Description = event.Description.right(event.Description.length()-position-7);
    }

    // Check for (CC) in the decription and set the <subtitles type="teletext"> flag
    position = event.Description.find("(CC)");
    if (position != -1)
    {
       event.SubTitled = true;
       event.Description = event.Description.replace("(CC)","");
    }

    // Check for (Stereo) in the decription and set the <audio> tags
    position = event.Description.find("(Stereo)");
    if (position != -1)
    {
       event.Stereo = true;
       event.Description = event.Description.replace("(Stereo)","");
    }

}

void SIParser::EITFixUpStyle2(Event& event)
{
    int16_t position = event.Description.find("New Series");
    if (position != -1)
    {
        //Do something here
    }
    //BBC three case (could add another record here ?)
    event.Description = event.Description.replace(" Then 60 Seconds.","");
    event.Description = event.Description.replace(" Followed by 60 Seconds.","");
    event.Description = event.Description.replace("Brand New Series - ","");
    event.Description = event.Description.replace("Brand New Series","");
    event.Description = event.Description.replace("New Series","");

    position = event.Description.find(':');
    if (position != -1)
    {
        event.Event_Subtitle = event.Description.left(position);
        event.Description = event.Description.right(event.Description.length()-position-2);
        if ((event.Event_Subtitle.length() > 0) &&
            (event.Description.length() > 0) &&
            (event.Event_Subtitle.length() > event.Description.length()))
        {
            QString Temp = event.Event_Subtitle;
            event.Event_Subtitle = event.Description;
            event.Description = Temp;
        }
    }
}

void SIParser::EITFixUpStyle3(Event& event)
{
    /* Used for PBS ATSC Subtitles are seperated by a colon */
    int16_t position = event.Description.find(':');
    if (position != -1)
    {
        event.Event_Subtitle = event.Description.left(position);
        event.Description = event.Description.right(event.Description.length()-position-2);
    }
}

void SIParser::EITFixUpStyle4(Event& event)
{
    // Used for swedish dvb cable provider ComHem

    // the format of the subtitle is:
    // country. category. year.
    // the year is optional and if the subtitle is empty the same information is
    // in the Description instead.
    if (event.Event_Subtitle.length() == 0 && event.Description.length() > 0)
    {
        event.Event_Subtitle = event.Description;
        event.Description = "";
    }

    // try to find country category and year
    QRegExp rx("^(.+)\\.\\s(.+)\\.\\s(?:([0-9]{2,4})\\.\\s*)?");
    int pos = rx.search(event.Event_Subtitle);
    if (pos != -1)
    {
        QStringList list = rx.capturedTexts();

        // sometimes the category is empty, in that case use the category from
        // the one from subtitle. this category is in swedish and all others 
        // are in english
        if (event.ContentDescription.length() == 0)
        {
            event.ContentDescription = list[2].stripWhiteSpace();
        }

        if (list[3].length() > 0)
        {
            event.Year = list[3].stripWhiteSpace();
        }

        // not 100% sure about this one.
        event.Event_Subtitle="";
    }

    if (event.Description.length() > 0)
    {
        // everything up to the first 3 spaces is duplicated from title and
        // subtitle so remove it
        int pos = event.Description.find("   ");
        if (pos != -1)
        {
            event.Description = event.Description.mid(pos + 3).stripWhiteSpace();
            //fprintf(stdout,"SIParser::EITFixUpStyle4: New: %s\n",event.Description.mid(pos+3).stripWhiteSpace().ascii());
        }

        // in the description at this point everything up to the first 4 spaces
        // in a row is a list of director(s) and actors.
        // different lists are separated by 3 spaces in a row
        // end of all lists is when there is 4 spaces in a row

        /*
        a regexp like this coud be used to get the episode number, shoud be at
        the begining of the description
        "^(?:[dD]el|[eE]pisode)\\s([0-9]+)(?:(?:\\s|/)av\\s([0-9]+))?\\."
        */

        /*
        we coud also tell if this show is a rerun and when it was last shown
        by looking for "Repris från day/month."
        and future showings by looking for "Även day/month."
        */
    }
}

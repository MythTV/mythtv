#include <pthread.h>
#include <stdio.h>
#include <qmutex.h>
#include "siscan.h"
#include "dvbchannel.h"
#include "siparser.h"
#include "dvbtypes.h"
#include "libmythtv/scheduledrecording.h"
#include "frequencies.h"
#include "mythdbcon.h"


#define CHECKNIT_TIMER 5000
//#define SISCAN_DEBUG

SIScan::SIScan(DVBChannel* advbchannel, int _sourceID)
{
    /* setup links to parents and db connections */
    chan = advbchannel;
    sourceID = _sourceID;

    /* setup boolean values for thread */
    scannerRunning = false;
    serviceListReady = false;
    transportListReady = false;
    eventsReady = false;

    scanMode = IDLE;
    ScanTimeout = 0; 		/* Set scan timeout off by default */

    FTAOnly = false;
    forceUpdate = false;

    pthread_mutex_init(&events_lock, NULL);

    /* Signals to process tables and do database inserts */
    connect(chan->siparser,SIGNAL(FindTransportsComplete()),this,SLOT(TransportTableComplete()));
    connect(chan->siparser,SIGNAL(FindServicesComplete()),this,SLOT(ServiceTableComplete()));
    connect(chan->siparser,SIGNAL(EventsReady(QMap_Events*)),this,SLOT(EventsReady(QMap_Events*)));
}

SIScan::~SIScan()
{
    while(scannerRunning)
       usleep(50);
    SIPARSER("SIScanner Stopped");

    pthread_mutex_destroy(&events_lock);
}

void SIScan::SetSourceID(int _SourceID)
{
    // Todo: this needs to be used more so you don't get data fubared running in auto-scan mode
    sourceID = _SourceID;
}

bool SIScan::ScanTransports()
{
    return chan->siparser->FindTransports();
}

bool SIScan::ScanServicesSourceID(int SourceID)
{

    if (scanMode == TRANSPORT_LIST)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    /* Run DB query to get transports on sourceid SourceID connected to this card */
    QString theQuery = QString("select mplexid, sistandard, transportid from dtv_multiplex where "
                       "sourceid = %1")
                       .arg(SourceID);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Get Transports for SourceID", query);

    if (query.size() <= 0)
    {
        SISCAN(QString("Unable to find any transports for sourceId %1.").arg(sourceID));
        return false;
    }

    scanTransports.clear();

    transportsCount = transportsToScan = query.size();
    int tmp = transportsToScan;
    while (tmp--)
    {
        query.next();
        if (query.value(1).toString() == QString("atsc"))
            ScanTimeout = CHECKNIT_TIMER;
        TransportScanList t;
        t.mplexid = query.value(0).toInt();
        t.SourceID = SourceID;
        t.FriendlyName = QString("Transport ID %1").arg(query.value(2).toString());
        scanTransports += t;
    }

    sourceIDTransportTuned = false;
    scanMode = TRANSPORT_LIST;    
    return true;
}

// This function requires you be tuned to a transport already
bool SIScan::ScanServices(int TransportID)
{
    (void) TransportID;
    // Requests the SDT table for the current transport from sections
    return chan->siparser->FindServices();
}

void SIScan::StartScanner()
{
    SIPARSER("Starting SIScanner");
    scannerRunning = true;
    threadExit = false;

    while (!threadExit)
    {
        usleep(250);
        if (eventsReady)
        {
            SISCAN(QString("Got an EventsQMap to process"));
            pthread_mutex_lock(&events_lock);
            bool ready = eventsReady;
            eventsReady = false;
            pthread_mutex_unlock(&events_lock);
            if (ready)
                AddEvents();
        }

        if (serviceListReady)
        {
            if (chan->siparser->GetServiceObject(SDT))
            {
                SISCAN("Updating Services");
                emit ServiceScanUpdateText("Processing new Transport");
                UpdateServicesInDB(SDT);
                SISCAN("Service Update Complete");
                emit ServiceScanUpdateText("Finished processing Transport");
            }
            serviceListReady = false;
            if (scanMode == TRANSPORT_LIST)
            {

                /* We could just look at the transportsCount here but it's not really thread safe */
/*                QValueList<TransportScanList>::Iterator i;
                bool fAll = true;
                for (i = scanTransports.begin() ; i != scanTransports.end() && fAll ; ++i)
                    if (!(*i).complete)
                        fAll = false;
                if (fAll)
                    emit ServiceScanComplete();
*/
                sourceIDTransportTuned = false;
            }
            else
            {
                emit PctServiceScanComplete(100);
                emit ServiceScanComplete();
            }
        }
        if (transportListReady)
        {
            if (chan->siparser->GetTransportObject(NIT))
            {
                emit TransportScanUpdateText("Processing Transport List");
                SISCAN("Updating Transports");
                CheckNIT(NIT);
                UpdateTransportsInDB(NIT);
                SISCAN("Transport Update Complete");
                emit TransportScanUpdateText("Finished processing Transport List");
            }
            transportListReady = false;
            emit TransportScanComplete();
        }

        if (scanMode == TRANSPORT_LIST)
        {
            if (transportsCount == 0)
            {
                emit PctServiceScanComplete(100);
                emit ServiceScanComplete();
                scanMode = IDLE; 
            }
            if ((!sourceIDTransportTuned) || ((ScanTimeout > 0) && (timer.elapsed() > ScanTimeout)))
            {
                QValueList<TransportScanList>::Iterator i;
                bool done = false;
                for (i = scanTransports.begin() ; i != scanTransports.end() ; ++i)
                {
                    if (!done && !(*i).complete && !threadExit)
                    {
                        (*i).complete = true;
                        if ((ScanTimeout > 0) && (timer.elapsed() > ScanTimeout))
                            emit ServiceScanUpdateText("Timeout Scaning Channel");
                        emit ServiceScanUpdateText((*i).FriendlyName);
                        SISCAN(QString("Tuning to mplexid %1").arg((*i).mplexid));
                        transportsCount--;
                        emit PctServiceScanComplete(((transportsToScan-transportsCount-1)*100)/transportsToScan);
         
                        bool result = false;                
                        if ((*i).mplexid == -1)
                        {
                            /* For ATSC / Cable try for 2 seconds to tune otherwise give up */
                            dvb_channel_t t;
                            t.tuning = (*i).tuning;
                            t.sistandard = "atsc";
                            result = chan->TuneTransport(t,true,2000);
                        }
                        else 
                            result = chan->SetTransportByInt((*i).mplexid);

                        if (!result)
                            emit ServiceScanUpdateText("Failed to tune");
                        else
                        {
                            if ((*i).mplexid == -1)
                            {
                                MSqlQuery query(MSqlQuery::InitCon());
                                query.prepare("INSERT into dtv_multiplex (frequency, "
                                               "modulation, sistandard, sourceid) "
                                               "VALUES (:FREQUENCY,:MODULATION,\"atsc\",:SOURCEID);");
                                query.bindValue(":FREQUENCY",(*i).Frequency);
                                query.bindValue(":MODULATION",(*i).Modulation);
                                query.bindValue(":SOURCEID",(*i).SourceID);
                                if(!query.exec())
                                    MythContext::DBError("Inserting new transport", query);
                                if (!query.isActive())
                                     MythContext::DBError("Adding transport to Database.", query);

                                query.prepare("select max(mplexid) from dtv_multiplex;");
                                if(!query.exec())
                                    MythContext::DBError("Getting ID of new Transport", query);
                                if (!query.isActive())
                                    MythContext::DBError("Getting ID of new Transport.", query);

                                if (query.size() > 0)
                                {
                                    query.next();
                                    chan->SetCurrentTransportDBID(query.value(0).toInt());
                                }

                            }
                            timer.start();
                            chan->siparser->FindServices();
                            sourceIDTransportTuned = true;
                            done = true;
                        }
                    }
                }
            }
        }
    }
    scannerRunning = false;
}

void SIScan::StopScanner()
{
    threadExit = true;
    /* Force dvbchannel to exit */
    chan->StopTuning();
    SIPARSER("Stopping SIScanner");
    while (scannerRunning)
       usleep(50);

}

bool SIScan::ATSCScanTransport(int SourceID, int FrequencyBand)
{

    if (scanMode == TRANSPORT_LIST)
        return false;

    struct CHANLIST* curList;
    QString Modulation;
    QString NamePrefix;

    switch (FrequencyBand)
    {
        case 0:
                   curList = chanlists[0].list;
                   /* Ensure scan only does 2-69 */
                   transportsCount = transportsToScan = 68;
                   Modulation = "8vsb";
                   NamePrefix = "ATSC Channel";
                   break;
        case 1:
                   curList = chanlists[1].list;
                   transportsCount = transportsToScan = chanlists[1].count; 
                   Modulation = "qam_256";
                   NamePrefix = "QAM Channel";
                   break;
        default:
                   return false;
    }

    scanTransports.clear();
	
    /* Now generate a list of frequencies to scan and add it to the atscScanTransportList */

    MSqlQuery query(MSqlQuery::InitCon());

    for (int x = 0 ; x < transportsToScan ; x++)
    {
        TransportScanList a;
        uint32_t freq = (curList[x].freq + 1750) * 1000;
        a.FriendlyName = QString("%1 %2").arg(NamePrefix).arg(curList[x].name);
        QString Frequency = QString("%1").arg(freq);
        a.Frequency = Frequency;
        a.Modulation = Modulation;
        a.SourceID = SourceID;
        chan->ParseATSC(Frequency,Modulation,a.tuning);

        /* See if mplexid is already in the database */
        QString theQuery = QString("select mplexid from dtv_multiplex where "
                           "sourceid = %1 and frequency = %2")
                           .arg(SourceID)
                           .arg(Frequency);

        query.prepare(theQuery);
    
        if (!query.exec() || !query.isActive())
            MythContext::DBError("Check for existing transport", query);
    
        if (query.size() <= 0)
            a.mplexid = -1;
        else
        {
            query.next();
            a.mplexid = query.value(0).toInt();            
        }
        scanTransports += a;

    }    

    timer.start();
    ScanTimeout = CHECKNIT_TIMER;
    sourceIDTransportTuned = false;
    scanMode = TRANSPORT_LIST;    
    return true;
}

void SIScan::EventsReady(QMap_Events* EventList)
{
    // Extra check since adding events to the DB is SLOW..
    if (threadExit)
        return;
    QList_Events* events = new QList_Events();
    pthread_mutex_lock(&events_lock);
    QMap_Events::Iterator e;
    for (e = EventList->begin() ; e != EventList->end() ; ++e)
        events->append(*e);
    eventsReady = true;
    Events.append(events);
    pthread_mutex_unlock(&events_lock);
}

void SIScan::ServiceTableComplete()
{
    serviceListReady = true;
}

void SIScan::TransportTableComplete()
{
    transportListReady = true;
}

void SIScan::UpdateServicesInDB(QMap_SDTObject SDT)
{

    /* This will be fixed post .17 to be more elegant */
    int localSourceID = -1;

    if (SDT.empty())
    {
        SISCAN("SDT List Empty assuming this is by error leaving services intact");
        return;
    }

    QMap_SDTObject::Iterator s;
    int chanid=0;

/*


       If the mplexid matches the first networkid/transport ID assume you are on the right
       mplexid..

       If NOT then search that SourceID for a matching NetworkId/TransportID and use that
       as the mplexid..


    3. Check the existnce of each ServiceID on the correct mplexid.  If there are any extras mark
       then as hidden (as opposed to removing them).  There migth also be a better way of marking
       them as old.. I am not sure yet.. If the ChanID scheme is better than whats in place now
       this might be easier..

    4. Add any missing ServiceIDs.. Set the hidden flag if they are not a TV service, and or if
       they are not FTA (Depends on if FTA is allowed or not)..

*/

/*  First get the mplexid from channel.. This will help you determine what mplexid these
    channels are really associated with.. They COULD be from another transport..   */

    s = SDT.begin();

    int DVBTID = GetDVBTID((*s).NetworkID,(*s).TransportID,chan->GetCurrentTransportDBID());

//    int DVBTID = chan->GetCurrentTransportDBID();

#ifdef  SISCAN_DEBUG
    printf("The mplexid is %d\n",DVBTID);
#endif
    if (DVBTID == -1)
    {
        SISCAN("Error determing what transport this service table is associated with so failing");
        return;
    }
/*  Check the ServiceVersion and if its the same as you have, or a 
    forced update is required because its been too long since an update 
    forge ahead.

    TODO: If you get a Version mismatch and you are not in setupscan mode you will want to issue a
          scan of the other transports that have the same networkID */

    MSqlQuery query(MSqlQuery::InitCon());
    QString versionQuery = QString("SELECT serviceversion,sourceid FROM dtv_multiplex "
                                   "WHERE mplexid = %1")
                                   .arg(DVBTID);
    query.prepare(versionQuery);

    if(!query.exec())
        MythContext::DBError("Selecting channel/dtv_multiplex", query);

    if (!query.isActive())
         MythContext::DBError("Check Channel full in channel/dtv_multiplex.", query);


    if (query.size() > 0)
    {
        query.next();

        localSourceID = query.value(1).toInt();

        if (!forceUpdate && (query.value(0).toInt() == (*s).Version))
        {
            SISCAN("Service table up to date for this network.");
            emit ServiceScanUpdateText("Channels up to date");
            return;
        }
        else
        {
            versionQuery = QString("UPDATE dtv_multiplex SET serviceversion = %1 "
                                   "WHERE mplexid = %2")
                                   .arg((*s).Version)
                                   .arg(DVBTID);
            query.prepare(versionQuery);

            if(!query.exec())
                MythContext::DBError("Selecting channel/dtv_multiplex", query);

            if (!query.isActive())
                MythContext::DBError("Check Channel full in channel/dtv_multiplex.", query);
        }
    }

// Get the freetoaironly field from cardinput 

    QString FTAQuery = QString("SELECT freetoaironly from cardinput,dtv_multiplex, "
                               "capturecard WHERE "
                               "cardinput.sourceid = dtv_multiplex.sourceid AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "mplexid=%1 and videodevice = \"%2\"")
                               .arg(DVBTID)
                               .arg(chan->GetCardNum());
    query.prepare(FTAQuery); 

    if(!query.exec())      
        MythContext::DBError("Getting FreeToAir for cardinput", query);
            
    if (!query.isActive())
        MythContext::DBError("Getting FreeToAir for cardinput", query);

    if (query.size() > 0)
    {
        query.next();
        FTAOnly = query.value(0).toInt();
    }
    else
    {
        return;    
    }

// TODO: Process Steps 3 and 4.. Only a partial 4 is being done now..
    for (s = SDT.begin() ; s != SDT.end() ; ++s )
    {
        // Its a TV Service so add it also only do FTA
        if(((*s).ServiceType == 1) &&
           (!FTAOnly || (FTAOnly && ((*s).CAStatus == 0))))
        {
            // See if transport already in database based on serviceid,networkid  and transportid
            QString theQuery = QString("SELECT chanid FROM channel "
                                       "WHERE serviceID=%1 and mplexid=%2")
                                       .arg((*s).ServiceID)
                                       // Use a different style query here in the future when you are
                                       // parsing other transports..  This will work for now..
                                       .arg(chan->GetCurrentTransportDBID());
            query.prepare(theQuery);

            if(!query.exec())
                MythContext::DBError("Selecting channel/dtv_multiplex", query);

            if (!query.isActive())
                MythContext::DBError("Check Channel full in channel/dtv_multiplex.", query);

            // If channel not present add it
            if (query.size() <= 0)
            {

                    if ((*s).ServiceName.stripWhiteSpace().isEmpty())
                    {
                        (*s).ServiceName = "Unnamed Channel";
                    }

                    SISCAN(QString("Channel %1 Not Found in Database - Adding").arg((*s).ServiceName));
                    QString status = QString("Adding %1").arg((*s).ServiceName);
                    emit ServiceScanUpdateText(status);
                    // Get next chanid and determine what Transport its on

                    chanid = GenerateNewChanID();

                    int ChanNum;
                    if( (*s).ChanNum == -1 )
                        ChanNum = (*s).ServiceID;
                    else
                        ChanNum = (*s).ChanNum;

                     query.prepare("INSERT INTO channel (chanid, channum, "
                               "sourceid, callsign, name,  mplexid, "
                               "serviceid, atscsrcid, useonairguide ) "
                               "VALUES (:CHANID,:CHANNUM,:SOURCEID,:CALLSIGN,"
                               ":NAME,:MPLEXID,:SERVICEID,:ATSCSRCID,:USEOAG);");
                     query.bindValue(":CHANID",chanid);
                     query.bindValue(":CHANNUM",ChanNum);
                     query.bindValue(":SOURCEID",localSourceID);
                     query.bindValue(":CALLSIGN",(*s).ServiceName.utf8());
                     query.bindValue(":NAME",(*s).ServiceName.utf8());
                     query.bindValue(":MPLEXID",chan->GetCurrentTransportDBID());
                     query.bindValue(":SERVICEID",(*s).ServiceID);
                     query.bindValue(":ATSCSRCID",(*s).ATSCSourceID);
                     query.bindValue(":USEOAG",(*s).EITPresent);

                     if(!query.exec())
                        MythContext::DBError("Adding new DVB Channel", query);

                     if (!query.isActive())
                         MythContext::DBError("Adding new DVB Channel", query);

                }
                else
                {
                    // TODO: Do an update here of the Name, etc..
                    SISCAN(QString("Channel %1 already in Database - Skipping").arg((*s).ServiceName));
                    QString status = QString("Updating %1").arg((*s).ServiceName);
                    emit ServiceScanUpdateText(status);
                }
            }
            else
            {
// These channels need to be added but have their type set to hidden or something like that
                QString status;
                if((*s).ServiceType == 1)
                    status = QString("Skipping %1 - Encrypted Service")
                                     .arg((*s).ServiceName);
                else
                    status = QString("Skipping %1 not a Television Service")
                                     .arg((*s).ServiceName);
 
                SISCAN(status);
                emit ServiceScanUpdateText(status);
            }
    }

}

void SIScan::CheckNIT(NITObject& NIT)
{
    dvb_channel_t chan_opts;
    QValueList<TransportObject>::Iterator t;
    for (t = NIT.Transport.begin() ; t != NIT.Transport.end() ; ++t )
    {
        TransportObject& transport = *t;
        if (transport.frequencies.empty())
            continue;
        //for each transport freqency list entry, try to tune to it
        //if it works use it as the frequency, we should probably try
        //and work out the strongest signal.

        //Parse the default transport object
        if (transport.Type == "DVB-T")
        {
            if (!chan->ParseOFDM(QString::number(transport.Frequency),
                             transport.Inversion,
                             transport.Bandwidth,
                             transport.CodeRateHP,
                             transport.CodeRateLP,
                             transport.Constellation,
                             transport.TransmissionMode,
                             transport.GuardInterval,
                             transport.Hiearchy,
                             chan_opts.tuning))
            {
                //cerr << "failed to parse transport\n";
                break;
            }
        }
        else if (transport.Type == "DVB-C")
        {
            if (!chan->ParseQAM(QString::number(transport.Frequency),
                             transport.Inversion,
                             QString::number(transport.SymbolRate),
                             transport.FEC_Inner,
                             transport.Modulation,
                             chan_opts.tuning))
            {
                //cerr << "failed to parse transport\n";
                break;
            }
        }
        else  //Lets hope we never get here
            break;

        //Start off with the main frequency,
        if (chan->TuneTransport(chan_opts,true,CHECKNIT_TIMER))
        {
//             cerr << "Tuned to main frequency "<< transport.Frequency << "\n";
            continue;
        }

        //Now the other ones in the list
        QValueList<unsigned>::Iterator f;
        for (f=transport.frequencies.begin();f!=transport.frequencies.end();f++)
        {
            unsigned nfrequency = *f;
            chan_opts.tuning.params.frequency = nfrequency;
//            cerr << "CheckNIT " << nfrequency << "\n";
            if (chan->TuneTransport(chan_opts,true,CHECKNIT_TIMER))
            {
//                cerr << "Tuned frequency "<< nfrequency << "\n";
                transport.Frequency = nfrequency;
                break;
            }
        }
    }
}

void SIScan::UpdateTransportsInDB(NITObject NIT)
{
    QValueList<NetworkObject>::Iterator n;
    QValueList<TransportObject>::Iterator t;

    // TODO: Add in ServiceList stuff, and automaticly add those serviceIDs (how you get the
    //       the channel numbers for the UK DVB-T channels)

    n = NIT.Network.begin();

    if(!NIT.Network.empty())
    {
        SISCAN(QString("Network %1 Scanned").arg((*n).NetworkName));
        emit TransportScanUpdateText(QString("Network %1 Processing").arg((*n).NetworkName));
    }

    for (t = NIT.Transport.begin() ; t != NIT.Transport.end() ; ++t )
    {
        MSqlQuery query(MSqlQuery::InitCon());
        // See if transport already in database
        QString theQuery = QString("select * from dtv_multiplex where NetworkID = %1 and "
                   " TransportID = %2 and Frequency = %3 and sourceID = %4")
                   .arg((*t).NetworkID)
                   .arg((*t).TransportID)
                   .arg((*t).Frequency)
                   .arg(sourceID);
        query.prepare(theQuery);

        if(!query.exec())
            MythContext::DBError("Selecting transports", query);

        if (!query.isActive())
            MythContext::DBError("Check Transport in dtv_multiplex.", query);

        // If transport not present add it, and move on to the next
        if (query.size() <= 0)
        {
            emit TransportScanUpdateText(QString("Transport %1 - %2 Added")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            SISCAN(QString("Transport TID = %1 NID = %2 added to Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            query.prepare("INSERT into dtv_multiplex (transportid, "
                               "networkid, frequency, symbolrate, fec, "
                               "polarity, modulation, constellation, "
                               "bandwidth, hierarchy, hp_code_rate, "
                               "lp_code_rate, guard_interval, "
                               "transmission_mode, inversion, sourceid) "
                               "VALUES (:TRANSPORTID,:NETWORKID,:FREQUENCY,"
                               ":SYMBOLRATE,:FEC,:POLARITY,:MODULATION,"
                               ":CONSTELLATION,:BANDWIDTH,:HIERARCHY,"
                               ":HP_CODE_RATE,:LP_CODE_RATE,:GUARD_INTERVAL,"
                               ":TRANSMISSION_MODE,:INVERSION,:SOURCEID);");
                               query.bindValue(":TRANSPORTID",(*t).TransportID);
                               query.bindValue(":NETWORKID",(*t).NetworkID);
                               query.bindValue(":FREQUENCY",(*t).Frequency);
                               query.bindValue(":SYMBOLRATE",(*t).SymbolRate);
                               query.bindValue(":FEC",(*t).FEC_Inner);
                               query.bindValue(":POLARITY",(*t).Polarity);
                               query.bindValue(":MODULATION",(*t).Modulation);
                               query.bindValue(":CONSTELLATION",(*t).Constellation);
                               query.bindValue(":BANDWIDTH",(*t).Bandwidth);
                               query.bindValue(":HIERARCHY",(*t).Hiearchy);
                               query.bindValue(":HP_CODE_RATE",(*t).CodeRateHP);
                               query.bindValue(":LP_CODE_RATE",(*t).CodeRateLP);
                               query.bindValue(":GUARD_INTERVAL",(*t).GuardInterval);
                               query.bindValue(":TRANSMISSION_MODE",(*t).TransmissionMode);
                               query.bindValue(":INVERSION",(*t).Inversion);
                               query.bindValue(":SOURCEID",sourceID);
            if(!query.exec())
                MythContext::DBError("Inserting new transport", query);
            if (!query.isActive())
                MythContext::DBError("Adding transport to Database.", query);
        }
        else
        {
            emit TransportScanUpdateText(QString("Transport %1 - %2 Already in Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            SISCAN(QString("Transport TID = %1 NID = %2 already in Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));
        }
    }
}

int SIScan::GetDVBTID(uint16_t NetworkID,uint16_t TransportID,int CurrentMplexId)
{

#ifdef SISCAN_DEBUG
    printf("Request for Networkid/Transportid %d %d\n",NetworkID,TransportID);
#endif

    // See if you can get an exact match baed on the current mplexid's sourceID
    // and the NetworkID/TransportID

    // First see if current one is NULL, if so update those values and return current mplexid

    MSqlQuery query(MSqlQuery::InitCon());
    QString theQuery;

    theQuery = QString("select networkid,transportid from dtv_multiplex where "
                       "mplexid = %1")
                       .arg(CurrentMplexId);
    query.prepare(theQuery);

    if(!query.exec())
        MythContext::DBError("Getting mplexid global search", query);

    if (!query.isActive())
        MythContext::DBError("Getting mplexid global search", query);

    query.next();

#ifdef SISCAN_DEBUG
    printf("Query Results: %d %d\n",query.value(0).toInt(),query.value(1).toInt());
#endif

    if ((query.value(0).toInt() == NetworkID) && (query.value(1).toInt() == TransportID))
    {
        return CurrentMplexId;
    }

    if ((query.value(0).toInt() == 0) && (query.value(1).toInt() == 0))
    {
        theQuery = QString("UPDATE dtv_multiplex set networkid=%1,transportid=%2 where "
                           "mplexid = %3")
                           .arg(NetworkID)
                           .arg(TransportID)
                           .arg(CurrentMplexId);
        query.prepare(theQuery);

        if(!query.exec())
            MythContext::DBError("Getting mplexid global search", query);

        if (!query.isActive())
            MythContext::DBError("Getting mplexid global search", query);

        return CurrentMplexId;

    }

    // Values were set, so see where you can find this NetworkID/TransportID
    theQuery = QString("SELECT a.mplexid FROM "
                       "dtv_multiplex a, dtv_multiplex b WHERE "
                       "a.networkid=%1 and a.transportid=%2 and "
                       "a.sourceId=b.sourceID and b.mplexid=%3;")
                       .arg(NetworkID)
                       .arg(TransportID)
                       .arg(CurrentMplexId);
    query.prepare(theQuery);

    if(!query.exec())
        MythContext::DBError("Finding matching mplexid", query);

    if (!query.isActive())
        MythContext::DBError("Finding matching mplexid", query);

    query.next();

    // If you got an exact match just return it, since there is no question what
    // mplexid this NetworkId/TransportId is for.
    if (query.size() == 1)
    {
#ifdef SISCAN_DEBUG
        printf("Exact Match!\n");
#endif
        int tmp = query.value(0).toInt();
        return tmp;
    }

    // If you got more than 1 hit then all you can do is hope that this NetworkID/TransportID
    // pair belongs to the CurrentMplexId;
    if (query.size() > 1)
    {
#ifdef SISCAN_DEBUG
        printf("more than 1 hit\n");
#endif
        return CurrentMplexId;
    }

    // If you got here you didn't find any match at all.. Try to see if this N/T pair is
    // on a transport that we know about..  Search without the SourceID restriction..
    theQuery = QString("select mplexid from dtv_multiplex where "
                       "networkID = %1 and transportID = %2")
                       .arg(NetworkID)
                       .arg(TransportID);
    query.prepare(theQuery);

    if(!query.exec())
        MythContext::DBError("Getting mplexid global search", query);

    if (!query.isActive())
        MythContext::DBError("Getting mplexid global search", query);

    query.next();

    // If you still didn't find this combo return -1 (failure)
    if (query.size() <= 0)
    {
        return -1;
    }

    // Now you are in trouble.. You found more than 1 match, so just guess that its the first
    // entry in the database..
    if (query.size() > 1)
    {
        SISCAN(QString("Found more than 1 match for NetworkID %1 TransportID %2")
               .arg(NetworkID).arg(TransportID));
        int tmp = query.value(0).toInt();
        return tmp;
    }

    // You found the entries, but it was on a different sourceID, so return that value
    int retval = query.value(0).toInt();

    return retval;

}

int SIScan::GenerateNewChanID()
{

    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery = QString("select max(chanid) as maxchan from channel where sourceid=%1")
                       .arg(sourceID);
    query.prepare(query);

    if(!query.exec())
        MythContext::DBError("Calculating new ChanID", query);

    if (!query.isActive())
        MythContext::DBError("Calculating new ChanID for DVB Channel", query);

    query.next();

    // If transport not present add it, and move on to the next
    if (query.size() <= 0)
        return sourceID * 1000;

    int MaxChanID = query.value(0).toInt();

    if (MaxChanID == 0)
        return sourceID * 1000;
    else
        return MaxChanID + 1;

}

void SIScan::AddEvents()
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString theQuery;
    int counter = 0;

    pthread_mutex_lock(&events_lock);
    while (!Events.empty())
    {
        QList_Events *events = Events.first();
        Events.pop_front();
        pthread_mutex_unlock(&events_lock);

        // All events are assumed to be for the same channel here
        QList_Events::Iterator e;
        e = events->begin();

        // Now figure out the chanid for this networkid/serviceid/sourceid/transport?
       /* if the event is associated with an ATSC channel the linkage to chanid is different than DVB */
        if ((*e).ATSC == true)
        {
            theQuery = QString("select chanid,useonairguide from channel where atscsrcid=%1 and mplexid=%2")
                           .arg((*e).ServiceID)
                           .arg(chan->GetCurrentTransportDBID());
#ifdef SISCAN_DEBUG
             printf("CHANID QUERY: %s\n",theQuery.ascii());
#endif
        }
        else  /* DVB Link to chanid */
        {
            theQuery = QString("select chanid,useonairguide from channel,dtv_multiplex where serviceid=%1"
                           " and networkid=%2 and channel.mplexid = dtv_multiplex.mplexid")
                           .arg((*e).ServiceID)
                           .arg((*e).NetworkID);

#ifdef SISCAN_DEBUG
            printf("CHANID QUERY: %s\n",theQuery.ascii());
#endif
        }

        query.prepare(theQuery);

        if(!query.exec())
            MythContext::DBError("Looking up chanID", query);

        if (!query.isActive())
            MythContext::DBError("Looking up chanID", query);

        query.next();

        if (query.size() <= 0)
        {
            SISCAN("ChanID not found so event skipped");
            events->clear();
            delete events;
            continue;
        }

        int ChanID = query.value(0).toInt();
        // Check to see if we are interseted in this channel 
        if (!query.value(1).toBool())
        {
            events->clear();
            delete events;
            continue;
        }

#ifdef SISCAN_DEBUG
        printf("QUERY: %d Events found for ChanId=%d\n",events->size(),ChanID);
#endif
        for (e = events->begin() ; e != events->end() ; ++e)
        {

            query.prepare("select * from  program where chanid=:CHANID and "
                      "starttime=:STARTTIME and programid=:PROGRAMID;");
            query.bindValue(":CHANID",ChanID);
            query.bindValue(":STARTTIME",(*e).StartTime.toString(QString("yyyy-MM-dd hh:mm:00")));
            query.bindValue(":ENDTIME",(*e).EndTime.toString(QString("yyyy-MM-dd hh:mm:00")));
            query.bindValue(":PROGRAMID",(*e).EventID);

            if(!query.exec())
                MythContext::DBError("Checking Event", query);

            if (!query.isActive())
                MythContext::DBError("Checking Event", query);

            if (query.size() <= 0)
            {
                 counter++;

                 query.prepare("INSERT INTO program (chanid,starttime,endtime,"
                          "title,description,subtitle,category,programid,"
                          "stereo,closecaptioned,hdtv)"
                          "VALUES (:CHANID,:STARTTIME,:ENDTIME,:TITLE,:DESCRIPTION,:SUBTITLE,:CATEGORY,:PROGRAMID,:STEREO,:CLOSECAPTIONED,:HDTV);");
                query.bindValue(":CHANID",ChanID);
                query.bindValue(":STARTTIME",(*e).StartTime.toString(QString("yyyy-MM-dd hh:mm:00")));
                query.bindValue(":ENDTIME",(*e).EndTime.toString(QString("yyyy-MM-dd hh:mm:00")));
                query.bindValue(":TITLE",(*e).Event_Name.utf8());
                query.bindValue(":DESCRIPTION",(*e).Description.utf8());
                query.bindValue(":SUBTITLE",(*e).Event_Subtitle.utf8());
                query.bindValue(":CATEGORY",(*e).ContentDescription.utf8());
                query.bindValue(":PROGRAMID",(*e).EventID);
                query.bindValue(":STEREO",(*e).Stereo);
                query.bindValue(":CLOSECAPTIONED",(*e).SubTitled);
                query.bindValue(":HDTV",(*e).HDTV);

                if(!query.exec())
                    MythContext::DBError("Adding Event", query);

                if (!query.isActive())
                   MythContext::DBError("Adding Event", query);
            }
        }

        if (counter > 0)
        {
            SISCAN(QString("Added %1 events, running scheduler to check for updates").arg(counter));
            ScheduledRecording::signalChange(-1);
            gContext->LogEntry("DVB/ATSC Guide Scanner", LP_INFO,"Added some events", "");
        }
        events->clear();
        delete events;
        pthread_mutex_lock(&events_lock);
    }
    pthread_mutex_unlock(&events_lock);
}


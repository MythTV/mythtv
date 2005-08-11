// -*- Mode: c++ -*-

#ifndef USING_DVB
#error USING_DVB must be defined to compile eitscanner.cpp
#endif

#include "dvbchannel.h"
#include "dvbsiparser.h"
#include "dvbtypes.h"

#include "eitscanner.h"
#include "eithelper.h"

/** \class EITScanner
 *  \brief Acts as glue between DVBChannel, DVBSIParser, and EITHelper.
 *
 *  This is the class where the "EIT Crawl" should be implemented.
 *
 */

EITScanner::EITScanner()
    : QObject(NULL, "EITScanner"),
      channel(NULL), parser(NULL), eitHelper(new EITHelper()),
      exitThread(false)
{
    pthread_create(&eventThread, NULL, SpawnEventLoop, this);
}

EITScanner::~EITScanner()
{
    StopPassiveScan();
    exitThread = true;
    pthread_join(eventThread, NULL);

    delete eitHelper;
}

void EITScanner::ChannelChanged(dvb_channel_t&)
{
    eitHelper->ClearList();
}

/** \fn EITScanner::SpawnEventLoop(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call EITScanner::RunScanner().
 */
void *EITScanner::SpawnEventLoop(void *param)
{
    EITScanner *scanner = (EITScanner*) param;
    scanner->RunEventLoop();
    return NULL;
}

/** \fn EITScanner::RunEventLoop()
 *  \brief This runs the event loop for EITScanner until 'exitThread' is true.
 */
void EITScanner::RunEventLoop(void)
{
    exitThread = false;
    while (!exitThread)
    {
        usleep(2000); // 2 ms

        if (channel && !exitThread)
        {
            int mplex = channel->GetMultiplexID();
            if ((mplex > 0) && parser && eitHelper->GetListSize())
                eitHelper->ProcessEvents(mplex);
        }
    }
}

/** \fn EITScanner::StartPassiveScan(DVBChannel*, DVBSIParser*)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(DVBChannel *_channel, DVBSIParser *_parser)
{
    eitHelper->ClearList();
    parser  = _parser;
    channel = _channel;
    connect(parser,    SIGNAL(EventsReady(QMap_Events*)),
            eitHelper, SLOT(HandleEITs(QMap_Events*)));
    connect(channel,   SIGNAL(ChannelChanged(dvb_channel_t&)),
            this,      SLOT(ChannelChanged(dvb_channel_t&)));
}

/** \fn EITScanner::StopPassiveScan()
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopPassiveScan(void)
{
    eitHelper->disconnect();
    eitHelper->ClearList();

    channel = NULL;
    parser  = NULL;
}

#include <qapplication.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <qsqldatabase.h>
#include <qsocket.h>

#include <iostream>
using namespace std;

#include "tv_rec.h"
#include "osd.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "recordingprofile.h"
#include "util.h"
#include "programinfo.h"
#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "channel.h"
#include "commercial_skip.h"

void *SpawnEncode(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;
    nvr->StartRecording();
    return NULL;
}

TVRec::TVRec(int capturecardnum) 
{
    db_conn = NULL;
    channel = NULL;
    rbuffer = NULL;
    nvr = NULL;
    readthreadSock = NULL;
    readrequest = 0;
    readthreadlive = false;
    m_capturecardnum = capturecardnum;
    ispip = false;

    deinterlace_mode = 0;

    pthread_mutex_init(&db_lock, NULL);

    ConnectDB(capturecardnum);

    QString chanorder = gContext->GetSetting("ChannelOrdering", "channum + 0");

    audiosamplerate = -1;

    QString inputname, startchannel;

    GetDevices(capturecardnum, videodev, vbidev, audiodev, audiosamplerate,
               inputname, startchannel);

    channel = new Channel(this, videodev);
    channel->Open();
    channel->SetFormat(gContext->GetSetting("TVFormat"));
    channel->SetFreqTable(gContext->GetSetting("FreqTable"));
    if (inputname.isEmpty())
        channel->SetChannelByString(startchannel);
    else
        channel->SwitchToInput(inputname, startchannel);
    channel->SetChannelOrdering(chanorder);
    channel->Close();
}

void TVRec::Init(void)
{
    inoverrecord = false;
    overrecordseconds = gContext->GetNumSetting("RecordOverTime");

    nvr = NULL;
    rbuffer = NULL;

    internalState = nextState = kState_None; 

    runMainLoop = false;
    changeState = false;

    watchingLiveTV = false;

    pthread_create(&event, NULL, EventThread, this);

    while (!runMainLoop)
        usleep(50);

    curRecording = NULL;
    prevRecording = NULL;
    tvtorecording = 1;
}

TVRec::~TVRec(void)
{
    runMainLoop = false;
    pthread_join(event, NULL);

    if (channel)
        delete channel;
    if (rbuffer)
        delete rbuffer;
    if (nvr)
        delete nvr;
    if (db_conn)
        DisconnectDB();
}

void TVRec::StartRecording(ProgramInfo *rcinfo)
{  
    QString recprefix = gContext->GetSetting("RecordFilePrefix");

    if (inoverrecord)
    {
        nextState = RemoveRecording(internalState);
        changeState = true;

        while (changeState)
            usleep(50);
    }

    if (internalState == kState_None) 
    {
        outputFilename = rcinfo->GetRecordFilename(recprefix);
        recordEndTime = rcinfo->endts;
        curRecording = new ProgramInfo(*rcinfo);

        nextState = kState_RecordingOnly;
        changeState = true;
    }
    else if (internalState == kState_WatchingLiveTV)
    {
        outputFilename = rcinfo->GetRecordFilename(recprefix);
        recordEndTime = rcinfo->endts;
        curRecording = new ProgramInfo(*rcinfo);

        QString message = QString("LIVE_TV_READY %1").arg(m_capturecardnum);
        MythEvent me(message);

        gContext->dispatch(me);
    }  
}

void TVRec::StopRecording(void)
{
    if (StateIsRecording(internalState))
    {
        nextState = RemoveRecording(internalState);
        changeState = true;
        prematurelystopped = true;
    }
}

void TVRec::StateToString(TVState state, QString &statestr)
{
    switch (state) {
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded: statestr = "WatchingPreRecorded";
                                         break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
}

bool TVRec::StateIsRecording(TVState state)
{
    bool retval = false;

    if (state == kState_RecordingOnly || 
        state == kState_WatchingRecording)
    {
        retval = true;
    }

    return retval;
}

bool TVRec::StateIsPlaying(TVState state)
{
    bool retval = false;

    if (state == kState_WatchingPreRecorded || 
        state == kState_WatchingRecording)
    {
        retval = true;
    }

    return retval;
}

TVState TVRec::RemoveRecording(TVState state)
{
    if (StateIsRecording(state))
    {
        if (state == kState_RecordingOnly)
            return kState_None;
        return kState_WatchingPreRecorded;
    }
    return kState_Error;
}

TVState TVRec::RemovePlaying(TVState state)
{
    if (StateIsPlaying(state))
    {
        if (state == kState_WatchingPreRecorded)
            return kState_None;
        return kState_RecordingOnly;
    }
    return kState_Error;
}

void TVRec::WriteRecordedRecord(void)
{
    if (!curRecording)
        return;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    curRecording->WriteRecordedToDB(db_conn);
    pthread_mutex_unlock(&db_lock);

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);
}

void TVRec::HandleStateChange(void)
{
    bool changed = false;
    bool startRecorder = false;
    bool closeRecorder = false;
    bool killRecordingFile = false;

    QString statename;
    StateToString(nextState, statename);
    QString origname;
    StateToString(internalState, origname);

    if (nextState == kState_Error)
    {
        printf("ERROR: Attempting to set to an error state.\n");
        exit(-1);
    }

    if (internalState == kState_None && nextState == kState_WatchingLiveTV)
    {
        internalState = nextState;
        changed = true;

        startRecorder = true;

        watchingLiveTV = true;
    }
    else if (internalState == kState_WatchingLiveTV && 
             nextState == kState_None)
    {
        channel->StoreInputChannels();

        closeRecorder = true;

        internalState = nextState;
        changed = true;

        killRecordingFile = true;
        watchingLiveTV = false;
    }
    else if (internalState == kState_None && 
             nextState == kState_RecordingOnly) 
    {
        SetChannel(true);  
        rbuffer = new RingBuffer(outputFilename, true);

        WriteRecordedRecord();

        internalState = nextState;
        nextState = kState_None;
        changed = true;

        startRecorder = true;
    }   
    else if ((internalState == kState_RecordingOnly && 
              nextState == kState_None) ||
             (internalState == kState_WatchingRecording &&
              nextState == kState_WatchingPreRecorded))
    {
        closeRecorder = true;
        internalState = nextState;
        changed = true;
        inoverrecord = false;

        watchingLiveTV = false;
    }
    else if (internalState == kState_WatchingLiveTV &&
             nextState == kState_WatchingRecording)
    {
        channel->StoreInputChannels();

        nvr->Pause();
        while (!nvr->GetPause())
            usleep(5);

        rbuffer->Reset();
        SetChannel(false);

        nvr->Reset();
        nvr->TransitionToFile(outputFilename);

        nvr->Unpause();

        WriteRecordedRecord();

        internalState = nextState;
        changed = true;
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_WatchingLiveTV)
    {
        nvr->Pause(false);
        while (!nvr->GetPause())
            usleep(5);

        nvr->TransitionToRing();

        nvr->Unpause();

        int filelen = (int)(((float)nvr->GetFramesWritten() / frameRate));

        QString message = QString("DONE_RECORDING %1 %2").arg(m_capturecardnum)
                                                         .arg(filelen);
        MythEvent me(message);
        gContext->dispatch(me);

        delete curRecording;
        curRecording = NULL;

        MythEvent me2("RECORDING_LIST_CHANGE");
        gContext->dispatch(me2);

        outputFilename = gContext->GetSetting("LiveBufferDir") + 
                         QString("/ringbuf%1.nuv").arg(m_capturecardnum);

        inoverrecord = false;
        internalState = nextState;
        changed = true;
    }
    else if (internalState == kState_WatchingRecording &&
             nextState == kState_RecordingOnly)
    {
        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
    }
    else if (internalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
    {
        printf("Unknown state transition: %s to %s\n", origname.ascii(),
                                                       statename.ascii());
    }
    else
    {
        printf("Changing from %s to %s\n", origname.ascii(), statename.ascii());
    }
 
    if (startRecorder)
    {
        pthread_mutex_lock(&db_lock);

        MythContext::KickDatabase(db_conn);

        RecordingProfile profile;

        prematurelystopped = false;

        if (!profile.loadByName(db_conn, gContext->GetHostName()))
        {
            if (curRecording) 
            {
                int profileID = curRecording->GetScheduledRecording(db_conn)
                                            ->getProfileID();
                if (profileID > 0)
                    profile.loadByID(db_conn, profileID);
                else
                    profile.loadByName(db_conn, "Default");
            } 
            else 
            {
                profile.loadByName(db_conn, "Live TV");
            }
        }

        pthread_mutex_unlock(&db_lock);

        SetupRecorder(profile);

        pthread_create(&encode, NULL, SpawnEncode, nvr);

        while (!nvr->IsRecording())
            usleep(50);

        // evil.
        channel->SetFd(nvr->GetVideoFd());
        frameRate = nvr->GetFrameRate();
    }

    if (closeRecorder)
    {
        TeardownRecorder(killRecordingFile);
    }

    changeState = false;
}

void TVRec::SetupRecorder(RecordingProfile& profile) 
{
    nvr = new NuppelVideoRecorder();
    nvr->ChangeDeinterlacer(deinterlace_mode);
    nvr->SetRingBuffer(rbuffer);
    nvr->SetVideoDevice(videodev);
    nvr->SetVbiDevice(vbidev);

    QString setting = profile.byName("videocodec")->getValue();
    if (setting == "MPEG-4") {
      nvr->SetCodec("mpeg4");
      nvr->SetMP4TargetBitrate(profile.byName("mpeg4bitrate")->getValue().toInt());
      nvr->SetMP4ScaleBitrate(profile.byName("mpeg4scalebitrate")->getValue().toInt() ?
                              1 : 0);
      nvr->SetMP4Quality(profile.byName("mpeg4maxquality")->getValue().toInt(),
                         profile.byName("mpeg4minquality")->getValue().toInt(),
                         profile.byName("mpeg4qualdiff")->getValue().toInt());
      nvr->SetMP4OptionVHQ(profile.byName("mpeg4optionvhq")->getValue().toInt() ? 1 : 0);
      nvr->SetMP4OptionVHQ(profile.byName("mpeg4option4mv")->getValue().toInt() ? 1 : 0);
    } else if (setting == "RTjpeg") {
      nvr->SetCodec("rtjpeg");
      nvr->SetRTJpegQuality(profile.byName("rtjpegquality")->getValue().toInt());
      nvr->SetRTJpegMotionLevels(profile.byName("rtjpegchromafilter")->getValue().toInt(),
                                 profile.byName("rtjpeglumafilter")->getValue().toInt());
    } else if (setting == "Hardware MJPEG") {
      nvr->SetCodec("hardware-mjpeg");
      nvr->SetHMJPGQuality(profile.byName("hardwaremjpegquality")->getValue().toInt());
      nvr->SetHMJPGHDecimation(profile.byName("hardwaremjpeghdecimation")->getValue().toInt());
      nvr->SetHMJPGVDecimation(profile.byName("hardwaremjpegvdecimation")->getValue().toInt());
    } else {
      cerr << "Unknown video codec: " << setting << endl;
    }

    setting = profile.byName("audiocodec")->getValue();
    if (setting == "MP3") {
      nvr->SetAudioCompression(1);
      nvr->SetMP3Quality(profile.byName("mp3quality")->getValue().toInt());
      nvr->SetAudioSampleRate(profile.byName("samplerate")->getValue().toInt());
    } else if (setting == "Uncompressed") {
      nvr->SetAudioCompression(0);
    } else {
      cerr << "Unknown audio codec: " << setting << endl;
    }

    nvr->SetResolution(profile.byName("width")->getValue().toInt(),
                       profile.byName("height")->getValue().toInt());

    nvr->SetTVFormat(gContext->GetSetting("TVFormat"));
    nvr->SetVbiFormat(gContext->GetSetting("VbiFormat"));

    nvr->SetAudioDevice(audiodev);
   
    if (ispip)
    {
        nvr->SetAsPIP();
        nvr->SetResolution(160, 128);
        nvr->SetCodec("rtjpeg");
        nvr->SetRTJpegMotionLevels(0, 0);
        nvr->SetRTJpegQuality(255);
        nvr->SetAudioCompression(false);
    }
 
    nvr->Initialize();
}

void TVRec::TeardownRecorder(bool killFile)
{
    QMap<long long, int> blank_frame_map;
    QMap<long long, int> comm_breaks;

    int filelen = -1;

    if (prevRecording)
        delete prevRecording;

    if (curRecording)
        prevRecording = new ProgramInfo(*curRecording);
    else
        prevRecording = NULL;

    ispip = false;

    if (nvr)
    {
        filelen = (int)(((float)nvr->GetFramesWritten() / frameRate));

        QString message = QString("DONE_RECORDING %1 %2").arg(m_capturecardnum)
                                                         .arg(filelen);
        MythEvent me(message);
        gContext->dispatch(me);

        nvr->StopRecording();

        if (prevRecording && !killFile)
            nvr->GetBlankFrameMap(blank_frame_map);

        pthread_join(encode, NULL);
        delete nvr;
    }
    nvr = NULL;

    if (rbuffer)
    {
        if (readthreadlive)
            KillReadThread();

        delete rbuffer;
        rbuffer = NULL;
    }

    if (killFile)
    {
        unlink(outputFilename.ascii());
        if (curRecording)
        {
            delete curRecording;
            curRecording = NULL;
        }
        outputFilename = "";
    }
    else if (curRecording)
    {
        delete curRecording;
        curRecording = NULL;

        MythEvent me("RECORDING_LIST_CHANGE");
        gContext->dispatch(me);
    }

    if ((prevRecording) && (!killFile))
    {
        prevRecording->SetBlankFrameList(blank_frame_map, db_conn);

        if ((!prematurelystopped) &&
            (gContext->GetNumSetting("AutoCommercialFlag", 0)))
        {
            flagthreadstarted = false;

            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&commercials, &attr, FlagCommercialsThread, this);

            while (!flagthreadstarted)
                usleep(50);
        }

        delete prevRecording;
        prevRecording = NULL;
    }
}    

char *TVRec::GetScreenGrab(QString filename, int secondsin, int &bufferlen,
                           int &video_width, int &video_height)
{
    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    NuppelVideoPlayer *nupvidplay = new NuppelVideoPlayer();
    nupvidplay->SetRingBuffer(tmprbuf);
    nupvidplay->SetAudioSampleRate(gContext->GetNumSetting("AudioSampleRate"));

    char *retbuf = nupvidplay->GetScreenGrab(secondsin, bufferlen, video_width,
                                             video_height);

    delete nupvidplay;
    delete tmprbuf;

    return retbuf;
}

void TVRec::SetChannel(bool needopen)
{
    if (needopen)
        channel->Open();

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT channel.channum,cardinput.inputname "
                               "FROM channel,capturecard,cardinput WHERE "
                               "channel.chanid = %1 AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "cardinput.sourceid = %2 AND "
                               "capturecard.cardid = %3;")
                               .arg(curRecording->chanid)
                               .arg(curRecording->sourceid)
                               .arg(curRecording->cardid);

    QSqlQuery query = db_conn->exec(thequery);
    
    QString inputname = "";
    QString chanstr = "";
  
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        chanstr = query.value(0).toString();
        inputname = query.value(1).toString();
    } else {
        MythContext::DBError("SetChannel", query);
    }

    pthread_mutex_unlock(&db_lock);

    channel->SwitchToInput(inputname, chanstr);

    if (needopen)
        channel->Close();
}

void *TVRec::EventThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->RunTV();

    return NULL;
}

void *TVRec::FlagCommercialsThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->FlagCommercials();

    return NULL;
}

void TVRec::RunTV(void)
{ 
    paused = false;

    runMainLoop = true;
    exitPlayer = false;
    finishRecording = false;

    while (runMainLoop)
    {
        if (changeState)
            HandleStateChange();

        usleep(1000);

        if (StateIsRecording(internalState))
        {
            if (QDateTime::currentDateTime() > recordEndTime || finishRecording)
            {
                if (!inoverrecord && overrecordseconds > 0)
                {
                    recordEndTime = recordEndTime.addSecs(overrecordseconds);
                    inoverrecord = true;
                    //cout << "switching to overrecord for " 
                    //     << overrecordseconds << " more seconds\n";
                }
                else if (watchingLiveTV)
                {
                    nextState = kState_WatchingLiveTV;
                    changeState = true;
                }
                else
                {
                    nextState = RemoveRecording(internalState);
                    changeState = true;
                }
                finishRecording = false;
            }
        }

        if (exitPlayer)
        {
            if (internalState == kState_WatchingLiveTV)
            {
                nextState = kState_None;
                changeState = true;
            }
            else if (StateIsPlaying(internalState))
            {
                nextState = RemovePlaying(internalState);
                changeState = true;
            }
            exitPlayer = false;
        }
    }
  
    nextState = kState_None;
    HandleStateChange();
}

void TVRec::GetChannelInfo(Channel *chan, QString &title, QString &subtitle, 
                           QString &desc, QString &category, 
                           QString &starttime, QString &endtime, 
                           QString &callsign, QString &iconpath, 
                           QString &channelname, QString &chanid)
{
    title = " ";
    subtitle = " ";
    desc = " ";
    category = " ";
    starttime = " ";
    endtime = " ";
    callsign = " ";
    iconpath = " ";
    channelname = " ";
    chanid = " ";

    if (!db_conn)
        return;
    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    char curtimestr[128];
    time_t curtime;
    struct tm *loctime;

    curtime = time(NULL);
    loctime = localtime(&curtime);

    strftime(curtimestr, 128, "%Y%m%d%H%M%S", loctime);
   
    channelname = chan->GetCurrentName();
    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();
 
    QString thequery = QString("SELECT starttime,endtime,title,subtitle,"
                               "description,category,callsign,icon,"
                               "channel.chanid "
                               "FROM program,channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" "
                               "AND starttime < %2 AND endtime > %3 AND "
                               "program.chanid = channel.chanid AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%4\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%5\" AND "
                               "capturecard.hostname = \"%6\";")
                               .arg(channelname).arg(curtimestr).arg(curtimestr)
                               .arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    QString test;
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        starttime = query.value(0).toString();
        endtime = query.value(1).toString();
        test = query.value(2).toString();
        if (test != QString::null)
            title = QString::fromUtf8(test);
        test = query.value(3).toString();
        if (test != QString::null)
            subtitle = QString::fromUtf8(test);
        test = query.value(4).toString();
        if (test != QString::null)
            desc = QString::fromUtf8(test);
        test = query.value(5).toString();
        if (test != QString::null)
            category = QString::fromUtf8(test);
        test = query.value(6).toString();
        if (test != QString::null)
            callsign = test;
        test = query.value(7).toString();
        if (test != QString::null)
            iconpath = test;
        test = query.value(8).toString();
        if (test != QString::null)
            chanid = test;
    }
    else
    {
        // couldn't find a matching program for the current channel.
        // get the information about the channel anyway
        QString thequery = QString("SELECT callsign,icon,channel.chanid "
                                   "FROM channel,capturecard,cardinput "
                                   "WHERE channel.channum = \"%1\" AND "
                                   "channel.sourceid = cardinput.sourceid AND "
                                   "cardinput.inputname = \"%2\" AND "
                                   "cardinput.cardid = capturecard.cardid AND "
                                   "capturecard.videodevice = \"%3\" AND "
                                   "capturecard.hostname = \"%4\";")
                                   .arg(channelname)
                                   .arg(channelinput).arg(device)
                                   .arg(gContext->GetHostName());

        QSqlQuery query = db_conn->exec(thequery);

        QString test;
        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();

            test = query.value(0).toString();
            if (test != QString::null)
                callsign = test;
            test = query.value(1).toString();
            if (test != QString::null)
                iconpath = test;
            test = query.value(2).toString();
            if (test != QString::null)
                chanid = test;
        }
     }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::ConnectDB(int cardnum)
{
    QString name = QString("TV%1%2").arg(cardnum).arg(rand());

    pthread_mutex_lock(&db_lock);

    db_conn = QSqlDatabase::addDatabase("QMYSQL3", name);
    if (!db_conn)
    {
        pthread_mutex_unlock(&db_lock);
        printf("Couldn't initialize mysql connection\n");
        return;
    }
    if (!gContext->OpenDatabase(db_conn))
    {
        printf("Couldn't open database\n");
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::DisconnectDB(void)
{
    pthread_mutex_lock(&db_lock);

    if (db_conn)
    {
        db_conn->close();
        delete db_conn;
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::GetDevices(int cardnum, QString &video, QString &vbi, 
                       QString &audio, int &rate, QString &defaultinput,
                       QString &startchan)
{
    video = "";
    vbi = "";
    audio = "";
    defaultinput = "Television";
    startchan = "3";

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT videodevice,vbidevice,audiodevice,"
                               "audioratelimit,defaultinput FROM capturecard "
                               "WHERE cardid = %1;")
                              .arg(cardnum);

    QSqlQuery query = db_conn->exec(thequery);

    int testnum = 0;

    QString test;
    if (!query.isActive())
        MythContext::DBError("getdevices", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            video = QString::fromUtf8(test);
        test = query.value(1).toString();
        if (test != QString::null)
            vbi = QString::fromUtf8(test);
        test = query.value(2).toString();
        if (test != QString::null)
            audio = QString::fromUtf8(test);
        testnum = query.value(3).toInt();
        test = query.value(4).toString();
        if (test != QString::null)
            defaultinput = QString::fromUtf8(test);

        if (testnum > 0)
            rate = testnum;
        else
            rate = -1;
    }

    thequery = QString("SELECT if(startchan, startchan, '3') "
                       "FROM capturecard,cardinput WHERE inputname = \"%1\" "
                       "AND capturecard.cardid = %2 "
                       "AND capturecard.cardid = cardinput.cardid;")
                       .arg(defaultinput).arg(cardnum);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getstartchan", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            startchan = QString::fromUtf8(test);
    }

    pthread_mutex_unlock(&db_lock);
}

bool TVRec::CheckChannel(Channel *chan, const QString &channum, int &finetuning)
{
    finetuning = 0;

    if (!db_conn)
        return true;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();

    QString thequery = QString("SELECT channel.finetune FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("checkchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        finetuning = query.value(0).toInt();

        pthread_mutex_unlock(&db_lock);
        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    pthread_mutex_unlock(&db_lock);

    return ret;
}

bool TVRec::SetVideoFiltersForChannel(Channel *chan, const QString &channum)
{

    if (!db_conn)
        return true;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();
    QString videoFilters = "";

    QString thequery = QString("SELECT channel.videofilters FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("setvideofilterforchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        videoFilters = query.value(0).toString();

        if (videoFilters == QString::null) 
            videoFilters = "";

        if (nvr != NULL)
        {
            nvr->SetVideoFilters(videoFilters);
            nvr->InitFilters();
        }

        pthread_mutex_unlock(&db_lock);
        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    pthread_mutex_unlock(&db_lock);

    return ret;
}

int TVRec::GetChannelValue(const QString &channel_field,Channel *chan, 
                           const QString &channum)
{
    int retval = -1;

    if (!db_conn)
        return retval;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();
   
    QString thequery = QString("SELECT channel.%1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%2\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%3\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%4\" AND "
                               "capturecard.hostname = \"%5\";")
                               .arg(channel_field).arg(channum)
                               .arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getchannelvalue", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        retval = query.value(0).toInt();
    }

    pthread_mutex_unlock(&db_lock);
    return retval;
}

void TVRec::SetChannelValue(QString &field_name,int value, Channel *chan,
                            const QString &channum)
{

    if (!db_conn)
        return;

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();

    // Only mysql 4.x can do multi table updates so we need two steps to get 
    // the sourceid from the table join.
    QString thequery = QString("SELECT channel.sourceid FROM "
                               "channel,cardinput,capturecard "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);
    int sourceid = -1;

    if (!query.isActive())
        MythContext::DBError("setchannelvalue", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();
        sourceid = query.value(0).toInt();
    }

    if (sourceid != -1)
    {
        thequery = QString("UPDATE channel SET channel.%1=\"%2\" "
                           "WHERE channel.channum = \"%3\" AND "
                           "channel.sourceid = \"%4\";")
                           .arg(field_name).arg(value).arg(channum)
                           .arg(sourceid);
        query = db_conn->exec(thequery);
    }

    pthread_mutex_unlock(&db_lock);
}

QString TVRec::GetNextChannel(Channel *chan, int channeldirection)
{
    QString ret = "";

    // Get info on the current channel we're on
    QString channum = chan->GetCurrentName();
    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString channelorder = chan->GetOrdering();

    QString thequery = QString("SELECT %1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%2\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%3\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%4\" AND "
                               "capturecard.hostname = \"%5\";")
                               .arg(channelorder).arg(channum)
                               .arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    QString id = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        id = query.value(0).toString();
    }
    else
    {
        cerr << "Channel: \'" << channum << "\' was not found in the database.";
        cerr << "\nMost likely, the default channel set for this input\n";
        cerr << "(" << device << " " << channelinput << " )\n";
        cerr << "in setup is wrong\n";

        thequery = QString("SELECT %1 FROM channel,capturecard,cardinput "
                           "WHERE channel.sourceid = cardinput.sourceid AND "
                           "cardinput.inputname = \"%2\" AND "
                           "cardinput.cardid = capturecard.cardid AND "
                           "capturecard.videodevice = \"%3\" AND "
                           "capturecard.hostname = \"%4\" ORDER BY %5 "
                           "LIMIT 1;").arg(channelorder).arg(channelinput)
                           .arg(device).arg(gContext->GetHostName())
                           .arg(channelorder);
       
        query = db_conn->exec(thequery);

        if (query.isActive() && query.numRowsAffected() > 0)
        {
            query.next();
            id = query.value(0).toString();
        }
    }

    if (id == QString::null) {
        pthread_mutex_unlock(&db_lock);
        cerr << "Couldn't find any channels in the database, please make sure "
             << "\nyour inputs are associated properly with your cards.\n";
        return ret;
    }

    // Now let's try finding the next channel in the desired direction
    QString comp = ">";
    QString ordering = "";
    QString fromfavorites = "";
    QString wherefavorites = "";

    if (channeldirection == CHANNEL_DIRECTION_DOWN)
    {
        comp = "<";
        ordering = " DESC ";
    }
    else if (channeldirection == CHANNEL_DIRECTION_FAVORITE)
    {
        fromfavorites = ",favorites";
        wherefavorites = "AND favorites.chanid = channel.chanid";
    }

    QString wherepart = QString("channel.sourceid = cardinput.sourceid AND "
                                "cardinput.inputname = \"%1\" AND "
                                "cardinput.cardid = capturecard.cardid AND "
                                "capturecard.videodevice = \"%2\" AND "
                                "capturecard.hostname = \"%3\" ")
                                .arg(channelinput).arg(device)
                                .arg(gContext->GetHostName());

    thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                       "cardinput%1 WHERE "
                       "channel.%2 %3 \"%4\" %5 AND %6 "
                       "ORDER BY channel.%7 %8 LIMIT 1;")
                       .arg(fromfavorites).arg(channelorder)
                       .arg(comp).arg(id).arg(wherefavorites)
                       .arg(wherepart).arg(channelorder).arg(ordering);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("getnextchannel", query);
    else if (query.numRowsAffected() > 0)
    {
        query.next();

        ret = query.value(0).toString();
    }
    else
    {
        // Couldn't find the channel going in the desired direction, 
        // so loop around and find it on the flip side...
        comp = "<";
        if (channeldirection == CHANNEL_DIRECTION_DOWN) 
            comp = ">";

        // again, %9 is the limit for this
        thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                           "cardinput%1 WHERE "
                           "channel.%2 %3 \"%4\" %5 AND %6 "
                           "ORDER BY channel.%7 %8 LIMIT 1;")
                           .arg(fromfavorites).arg(channelorder)
                           .arg(comp).arg(id).arg(wherefavorites)
                           .arg(wherepart).arg(channelorder).arg(ordering);

        query = db_conn->exec(thequery);
 
        if (!query.isActive())
            MythContext::DBError("getnextchannel", query);
        else if (query.numRowsAffected() > 0)
        { 
            query.next();

            ret = query.value(0).toString();
        }
    }

    pthread_mutex_unlock(&db_lock);

    return ret;
}

bool TVRec::IsReallyRecording(void)
{
    if (nvr && nvr->IsRecording())
        return true;

    return false;
}

float TVRec::GetFramerate(void)
{
    return frameRate;
}

long long TVRec::GetFramesWritten(void)
{
    if (nvr)
        return nvr->GetFramesWritten();
    return -1;
}

long long TVRec::GetFilePosition(void)
{
    if (rbuffer)
        return rbuffer->GetTotalWritePosition();
    return -1;
}

long long TVRec::GetKeyframePosition(long long desired)
{
    if (nvr)
        return nvr->GetKeyframePosition(desired);
    return -1;
}

long long TVRec::GetFreeSpace(long long totalreadpos)
{
    if (rbuffer)
        return totalreadpos + rbuffer->GetFileSize() - 
               rbuffer->GetTotalWritePosition() - rbuffer->GetSmudgeSize();

    return -1;
}

void TVRec::TriggerRecordingTransition(void)
{
    if (!curRecording)
        return;

    if (internalState != kState_WatchingLiveTV)
        return;

    nextState = kState_WatchingRecording;
    changeState = true;
}

void TVRec::StopPlaying(void)
{
    exitPlayer = true;
}

void TVRec::SetupRingBuffer(QString &path, long long &filesize, 
                            long long &fillamount, bool pip)
{
    ispip = pip;
    filesize = gContext->GetNumSetting("BufferSize", 5);
    fillamount = gContext->GetNumSetting("MaxBufferFill", 50);

    path = gContext->GetSetting("LiveBufferDir") + QString("/ringbuf%1.nuv")
                                                       .arg(m_capturecardnum);

    outputFilename = path;

    filesize = filesize * 1024 * 1024 * 1024;
    fillamount = fillamount * 1024 * 1024;

    rbuffer = new RingBuffer(path, filesize, fillamount);
}

void TVRec::SpawnLiveTV(void)
{
    nextState = kState_WatchingLiveTV;
    changeState = true;

    while (changeState)
        usleep(50);
}

void TVRec::StopLiveTV(void)
{
    nextState = kState_None;
    changeState = true;

    while (changeState)
        usleep(50);
}

void TVRec::PauseRecorder(void)
{
    if (!nvr)
        return;

    nvr->Pause();
    while (!nvr->GetPause())
        usleep(5);

    PauseClearRingBuffer();
} 

void TVRec::ToggleInputs(void)
{
    rbuffer->Reset();

    channel->ToggleInputs();

    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

void TVRec::ChangeChannel(int channeldirection)
{
    rbuffer->Reset();
    
    if (channeldirection == CHANNEL_DIRECTION_FAVORITE)
        channel->NextFavorite();
    else if (channeldirection == CHANNEL_DIRECTION_UP)
        channel->ChannelUp();
    else
        channel->ChannelDown();

    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

void TVRec::ToggleChannelFavorite(void)
{
    // Get current channel id...
    QString channum = channel->GetCurrentName();
    QString channelinput = channel->GetCurrentInput();
    QString device = channel->GetDevice();

    pthread_mutex_lock(&db_lock);

    MythContext::KickDatabase(db_conn);

    QString thequery = QString("SELECT channel.chanid FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\" AND "
                               "capturecard.hostname = \"%4\";")
                               .arg(channum).arg(channelinput).arg(device)
                               .arg(gContext->GetHostName());

    QSqlQuery query = db_conn->exec(thequery);

    QString chanid = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        chanid = query.value(0).toString();
    }
    else
    {
        pthread_mutex_unlock(&db_lock);
        cerr << "Channel: \'" << channum << "\' was not found in the database.";
        cerr << "\nMost likely, your DefaultTVChannel setting is wrong.";
        cerr << "\nCould not toggle favorite.\n";
        return;
    }

    // Check if favorite exists for that chanid...
    thequery = QString("SELECT favorites.favid FROM favorites WHERE "
                       "favorites.chanid = \"%1\""
                       "LIMIT 1;")
                       .arg(chanid);

    query = db_conn->exec(thequery);

    if (!query.isActive())
        MythContext::DBError("togglechannelfavorite", query);
    else if (query.numRowsAffected() > 0)
    {
        // We have a favorites record...Remove it to toggle...
        query.next();
        QString favid = query.value(0).toString();

        thequery = QString("DELETE FROM favorites WHERE favid = \"%1\"")
                           .arg(favid);

        query = db_conn->exec(thequery);
        cout << "Removing Favorite.\n";
    }
    else
    {
        // We have no favorites record...Add one to toggle...
        thequery = QString("INSERT INTO favorites (chanid) VALUES (\"%1\")")
                           .arg(chanid);

        query = db_conn->exec(thequery);
        cout << "Adding Favorite.\n";
    }
    pthread_mutex_unlock(&db_lock);
}

int TVRec::ChangeContrast(bool direction)
{
    int ret = channel->ChangeContrast(direction);
    return ret;
}

int TVRec::ChangeBrightness(bool direction)
{
    int ret = channel->ChangeBrightness(direction);
    return ret;
}

int TVRec::ChangeColour(bool direction)
{
    int ret = channel->ChangeColour(direction);
    return ret;
}

void TVRec::ChangeDeinterlacer(int deint_mode)
{
    deinterlace_mode = deint_mode;
    return;
}

void TVRec::SetChannel(QString name)
{
    rbuffer->Reset();

    QString chan = name.stripWhiteSpace();
    QString prevchan = channel->GetCurrentName();

    if (!channel->SetChannelByString(chan))
        channel->SetChannelByString(prevchan);

    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
}

bool TVRec::CheckChannel(QString name)
{
    int finetune = 0;
    return CheckChannel(channel, name, finetune);
}

void TVRec::GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid)
{
    GetChannelInfo(channel, title, subtitle, desc, category, starttime,
                   endtime, callsign, iconpath, channelname, chanid);
}

void TVRec::GetInputName(QString &inputname)
{
    inputname = channel->GetCurrentInput();
}

void TVRec::PauseRingBuffer(void)
{
    rbuffer->StopReads();
    pthread_mutex_lock(&readthreadLock);
}

void TVRec::UnpauseRingBuffer(void)
{
    rbuffer->StartReads();
    pthread_mutex_unlock(&readthreadLock);
}

void TVRec::PauseClearRingBuffer(void)
{
    rbuffer->StopReads();
    pthread_mutex_lock(&readthreadLock);
}

long long TVRec::SeekRingBuffer(long long curpos, long long pos, int whence)
{
    if (!rbuffer)
        return -1;
    if (!readthreadlive)
        return -1;

    if (whence == SEEK_CUR)
    {
        long long realpos = rbuffer->GetTotalReadPosition();

        pos = pos + curpos - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    UnpauseRingBuffer();
    return ret;
}

void TVRec::SpawnReadThread(QSocket *sock)
{
    if (readthreadlive)
        return;

    pthread_mutex_init(&readthreadLock, NULL);
    readthreadSock = sock;
    readthreadlive = false;

    pthread_create(&readthread, NULL, ReadThread, this);

    while (!readthreadlive)
        usleep(50);
}

void TVRec::KillReadThread(void)
{
    if (readthreadlive)
    {
        readthreadlive = false;
        rbuffer->StopReads();
        pthread_join(readthread, NULL);
        readthreadSock = NULL;
    }
}

QSocket *TVRec::GetReadThreadSocket(void)
{
    return readthreadSock;
}

void TVRec::RequestRingBufferBlock(int size)
{
    if (!readthreadlive)
        return;

    if (size > 128000)
        size = 128000;

    pthread_mutex_lock(&readthreadLock);
    readrequest = size; 
    pthread_mutex_unlock(&readthreadLock);

    while (readrequest > 0 && readthreadlive)
    {
        qApp->unlock();
        usleep(500);
        qApp->lock();
    }
}

void TVRec::DoReadThread(void)
{
    readthreadlive = true;

    char *buffer = new char[128001];

    while (readthreadlive && rbuffer)
    {
        while (readrequest == 0 && readthreadlive)
            usleep(5000);

        if (!readthreadlive)
            break;

        pthread_mutex_lock(&readthreadLock);

        int ret = rbuffer->Read(buffer, readrequest);
        if (!rbuffer->GetStopReads())
            WriteBlock(readthreadSock, buffer, ret);

        readrequest -= ret;

        pthread_mutex_unlock(&readthreadLock);
    }

    delete [] buffer;
}

void *TVRec::ReadThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->DoReadThread();

    return NULL;
}

void TVRec::FlagCommercials(void)
{
    ProgramInfo *program_info = new ProgramInfo(*prevRecording);

    flagthreadstarted = true;

    nice(19);

    QString filename = program_info->GetRecordFilename(
                                gContext->GetSetting("RecordFilePrefix"));

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(db_conn, program_info);
    nvp->SetRingBuffer(tmprbuf);

    nvp->FlagCommercials();

    delete nvp;
    delete tmprbuf;
    delete program_info;
}

void TVRec::RetrieveInputChannels(map<int, QString> &inputChannel,
                                  map<int, QString> &inputTuneTo,
                                  map<int, QString> &externalChanger)
{
    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    QString query = QString("SELECT inputname, trim(externalcommand), "
                            "if(tunechan='', 'Undefined', tunechan), "
                            "if(startchan, startchan, '') "
                            "FROM capturecard, cardinput "
                            "WHERE capturecard.cardid = %1 "
                            "AND capturecard.cardid = cardinput.cardid;")
                            .arg(m_capturecardnum);

    QSqlQuery result = db_conn->exec(query);

    if (!result.isActive())
        MythContext::DBError("RetrieveInputChannels", result);
    else if (!result.numRowsAffected())
    {
        cerr << "Error getting inputs for the capturecard.  Perhaps you have\n"
                "forgotten to bind video sources to your card's inputs?\n";
    }
    else
    {
        int cap;

        while (result.next())
        {
            cap = channel->GetInputByName(result.value(0).toString());
            externalChanger[cap] = result.value(1).toString();
            inputTuneTo[cap] = result.value(2).toString();
            inputChannel[cap] = result.value(3).toString();
        }
    }

    pthread_mutex_unlock(&db_lock);
}

void TVRec::StoreInputChannels(map<int, QString> &inputChannel)
{
    QString query, input;

    pthread_mutex_lock(&db_lock);
    MythContext::KickDatabase(db_conn);

    for (int i = 0;; i++)
    {
        input = channel->GetInputByNum(i);
        if (input.isEmpty())
            break;

        query = QString("UPDATE cardinput set startchan = '%1' "
                        "WHERE cardid = %2 AND inputname = '%3';")
                        .arg(inputChannel[i]).arg(m_capturecardnum).arg(input);

        QSqlQuery result = db_conn->exec(query);

        if (!result.isActive())
            MythContext::DBError("StoreInputChannels", result);
    }

    pthread_mutex_unlock(&db_lock);
}


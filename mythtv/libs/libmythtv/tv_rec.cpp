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
#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"
#include "recordingprofile.h"
#include "libmyth/util.h"

void *SpawnEncode(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;
    nvr->StartRecording();
    return NULL;
}

TVRec::TVRec(MythContext *lcontext, const QString &startchannel, 
             int capturecardnum) 
{
    context = lcontext;
    db_conn = NULL;
    channel = NULL;
    rbuffer = NULL;
    nvr = NULL;
    readthreadSock = NULL;

    ConnectDB(capturecardnum);

    QString chanorder = context->GetSetting("ChannelOrdering");
    if (chanorder == "")
        chanorder = "channum + 0";

    audiosamplerate = -1;

    GetDevices(capturecardnum, videodev, audiodev, audiosamplerate);

    QString inputname = context->GetSetting("TunerCardInput");

    channel = new Channel(this, videodev);
    channel->Open();
    channel->SetFormat(context->GetSetting("TVFormat"));
    channel->SetFreqTable(context->GetSetting("FreqTable"));
    if (inputname != "")
        channel->SwitchToInput(inputname);
    channel->SetChannelByString(startchannel);
    channel->SetChannelOrdering(chanorder);
    channel->Close();
}

void TVRec::Init(void)
{
    inoverrecord = false;
    overrecordseconds = context->GetNumSetting("RecordOverTime");

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

int TVRec::AllowRecording(ProgramInfo *rcinfo, int timeuntil)
{
    if (internalState != kState_WatchingLiveTV)
    {
        return 1;
    }

    return -1;
}

void TVRec::StartRecording(ProgramInfo *rcinfo)
{  
    QString recprefix = context->GetSetting("RecordFilePrefix");

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

        if (internalState == kState_None)
            nextState = kState_RecordingOnly;
        else if (internalState == kState_WatchingPreRecorded)
            nextState = kState_WatchingOtherRecording;
        changeState = true;
    }
    else if (internalState == kState_WatchingLiveTV)
    {
        if (tvtorecording == 2)
        {
            nextState = kState_None;
            changeState = true;

            while (GetState() != kState_None)
                usleep(50);

            outputFilename = rcinfo->GetRecordFilename(recprefix);
            recordEndTime = rcinfo->endts;
            curRecording = new ProgramInfo(*rcinfo);

            nextState = kState_RecordingOnly;
            changeState = true;
        }
        else if (tvtorecording == 1)
        {
            outputFilename = rcinfo->GetRecordFilename(recprefix);
            recordEndTime = rcinfo->endts;
            curRecording = new ProgramInfo(*rcinfo);

            nextState = kState_WatchingRecording;
            changeState = true;
        }
        tvtorecording = 1;
    }  
}

void TVRec::StopRecording(void)
{
    if (StateIsRecording(internalState))
    {
        nextState = RemoveRecording(internalState);
        changeState = true;
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
        case kState_WatchingOtherRecording: statestr = "WatchingOtherRecording";
                                            break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        default: statestr = "Unknown"; break;
    }
}

bool TVRec::StateIsRecording(TVState state)
{
    bool retval = false;

    if (state == kState_RecordingOnly || 
        state == kState_WatchingRecording ||
        state == kState_WatchingOtherRecording)
    {
        retval = true;
    }

    return retval;
}

bool TVRec::StateIsPlaying(TVState state)
{
    bool retval = false;

    if (state == kState_WatchingPreRecorded || 
        state == kState_WatchingRecording ||
        state == kState_WatchingOtherRecording)
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

    curRecording->WriteRecordedToDB(db_conn);
    delete curRecording;
    curRecording = NULL;
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
        closeRecorder = true;

        internalState = nextState;
        changed = true;

        watchingLiveTV = false;
    }
    else if ((internalState == kState_None && 
              nextState == kState_RecordingOnly) || 
             (internalState == kState_WatchingPreRecorded &&
              nextState == kState_WatchingOtherRecording))
    {
        SetChannel(true);  
        rbuffer = new RingBuffer(context, outputFilename, true);

        internalState = nextState;
        nextState = kState_None;
        changed = true;

        startRecorder = true;
    }   
    else if ((internalState == kState_RecordingOnly && 
              nextState == kState_None) ||
             (internalState == kState_WatchingRecording &&
              nextState == kState_WatchingPreRecorded) ||
             (internalState == kState_WatchingOtherRecording &&
              nextState == kState_WatchingPreRecorded))
    {
        closeRecorder = true;

/*
        if (internalState == kState_WatchingRecording)
        {
            nvp->SetWatchingRecording(false);
            nvp->SetLength((int)(((float)nvr->GetFramesWritten() / frameRate)));
        }
*/
        internalState = nextState;
        changed = true;
        inoverrecord = false;

        watchingLiveTV = false;
    }
    else if (internalState == kState_None && 
             nextState == kState_None)
    {
        changed = true;
    }

    if (!changed)
    {
        printf("Unknown state transition: %d to %d\n", internalState,
                                                       nextState);
    }
    else
    {
        printf("Changing from %s to %s\n", origname.ascii(), statename.ascii());
    }
 
    if (startRecorder)
    {
        if (curRecording)
            SetupRecorder(curRecording->recordingprofileid);
        else
            SetupRecorder(1);

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

void TVRec::SetupRecorder(int profileid)
{
    if (nvr)
    {  
        printf("Attempting to setup a recorder, but it already exists\n");
        return;
    }

    nvr = new NuppelVideoRecorder();
    nvr->SetRingBuffer(rbuffer);
    nvr->SetVideoDevice(videodev);

    RecordingProfile profile(context);
    profile.loadByID(db_conn, profileid);

    QString setting = profile.byName("videocodec")->getValue();
    if (setting == "MPEG-4") {
      nvr->SetCodec("mpeg4");
      nvr->SetMP4TargetBitrate(profile.byName("mpeg4bitrate")->getValue().toInt());
      nvr->SetMP4ScaleBitrate(profile.byName("mpeg4scalebitrate")->getValue().isNull() ?
                              0 : 1);
      nvr->SetMP4Quality(profile.byName("mpeg4maxquality")->getValue().toInt(),
                         profile.byName("mpeg4minquality")->getValue().toInt(),
                         profile.byName("mpeg4qualdiff")->getValue().toInt());
    } else if (setting == "RTjpeg") {
      nvr->SetCodec("rtjpeg");
      nvr->SetRTJpegQuality(profile.byName("rtjpegquality")->getValue().toInt());
      nvr->SetRTJpegMotionLevels(profile.byName("rtjpegchromafilter")->getValue().toInt(),
                                 profile.byName("rtjpeglumafilter")->getValue().toInt());
    } else if (setting == "Hardware MJPEG") {
      nvr->SetHMJPGQuality(profile.byName("hardwaremjpegquality")->getValue().toInt());
      nvr->SetHMJPGDecimation(profile.byName("hardwaremjpegdecimation")->getValue().toInt());
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

    nvr->SetTVFormat(context->GetSetting("TVFormat"));

    nvr->SetAudioDevice(audiodev);
    
    nvr->Initialize();
}

void TVRec::TeardownRecorder(bool killFile)
{
    if (nvr)
    {
        nvr->StopRecording();
        pthread_join(encode, NULL);
        delete nvr;
    }
    nvr = NULL;

    if (rbuffer)
    {
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
    }
    else if (curRecording)
    {
        WriteRecordedRecord();
    }
}    

char *TVRec::GetScreenGrab(ProgramInfo *rcinfo, int secondsin, int &bufferlen,
                           int &video_width, int &video_height)
{
    QString recprefix = context->GetFilePrefix(); 
    QString filename = rcinfo->pathname;

    RingBuffer *tmprbuf = new RingBuffer(context, filename, false);

    NuppelVideoPlayer *nupvidplay = new NuppelVideoPlayer();
    nupvidplay->SetRingBuffer(tmprbuf);
    nupvidplay->SetAudioSampleRate(context->GetNumSetting("AudioSampleRate"));
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
    }

    channel->SwitchToInput(inputname);
    channel->SetChannelByString(chanstr);

    if (needopen)
        channel->Close();
}

void *TVRec::EventThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->RunTV();

    return NULL;
}

void TVRec::RunTV(void)
{ 
    paused = false;

    runMainLoop = true;
    exitPlayer = false;

    while (runMainLoop)
    {
        if (changeState)
            HandleStateChange();

        usleep(1000);

        if (StateIsRecording(internalState))
        {
            if (QDateTime::currentDateTime() > recordEndTime)
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
                           QString &channelname)
{
    title = "";
    subtitle = "";
    desc = "";
    category = "";
    starttime = "";
    endtime = "";

    if (!db_conn)
        return;

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
                               "description,category,callsign,icon "
                               "FROM program,channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" "
                               "AND starttime < %2 AND endtime > %3 AND "
                               "program.chanid = channel.chanid AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%4\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%5\";")
                               .arg(channelname).arg(curtimestr).arg(curtimestr)
                               .arg(channelinput).arg(device);

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
    }
}

void TVRec::ConnectDB(int cardnum)
{
    QString name = QString("TV%1%2").arg(cardnum).arg(rand());

    db_conn = QSqlDatabase::addDatabase("QMYSQL3", name);
    if (!db_conn)
    {
        printf("Couldn't initialize mysql connection\n");
        return;
    }
    if (!context->OpenDatabase(db_conn))
    {
        printf("Couldn't open database\n");
    }
}

void TVRec::DisconnectDB(void)
{
    if (db_conn)
    {
        db_conn->close();
        delete db_conn;
    }
}

void TVRec::GetDevices(int cardnum, QString &video, QString &audio, int &rate)
{
    video = "";
    audio = "";

    QString thequery = QString("SELECT videodevice,audiodevice,audioratelimit "
                               "FROM capturecard WHERE cardid = %1;")
                              .arg(cardnum);

    QSqlQuery query = db_conn->exec(thequery);

    int testnum = 0;

    QString test;
    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        test = query.value(0).toString();
        if (test != QString::null)
            video = QString::fromUtf8(test);
        test = query.value(1).toString();
        if (test != QString::null)
            audio = QString::fromUtf8(test);
        testnum = query.value(2).toInt();

        if (testnum > 0)
            rate = testnum;
        else
            rate = -1;
    }
}

bool TVRec::CheckChannel(Channel *chan, const QString &channum, int &finetuning)
{
    finetuning = 0;

    if (!db_conn)
        return true;

    bool ret = false;

    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();

    QString thequery = QString("SELECT channel.finetune FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%1\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%2\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%3\";")
                               .arg(channum).arg(channelinput).arg(device);

    QSqlQuery query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        finetuning = query.value(0).toInt();
        return true;
    }

    thequery = "SELECT NULL FROM channel;";
    query = db_conn->exec(thequery);

    if (query.numRowsAffected() == 0)
        ret = true;

    return ret;
}

QString TVRec::GetNextChannel(Channel *chan, bool direction)
{
    QString ret = "";

    QString channum = chan->GetCurrentName();
    QString channelinput = chan->GetCurrentInput();
    QString device = chan->GetDevice();

    QString channelorder = chan->GetOrdering();

    QString thequery = QString("SELECT %1 FROM "
                               "channel,capturecard,cardinput "
                               "WHERE channel.channum = \"%2\" AND "
                               "channel.sourceid = cardinput.sourceid AND "
                               "cardinput.inputname = \"%3\" AND "
                               "cardinput.cardid = capturecard.cardid AND "
                               "capturecard.videodevice = \"%4\";")
                               .arg(channelorder).arg(channum)
                               .arg(channelinput).arg(device);

    QSqlQuery query = db_conn->exec(thequery);

    QString id = QString::null;

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        id = query.value(0).toString();
    }

    if (id == QString::null)
        return ret;

    QString comp = ">";
    QString ordering = "";

    if (direction == false)
    {
        comp = "<";
        ordering = " DESC ";
    }

    thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                       "cardinput WHERE channel.%1 %2 \"%3\" AND "
                       "channel.sourceid = cardinput.sourceid AND "
                       "cardinput.inputname = \"%4\" AND "
                       "cardinput.cardid = capturecard.cardid AND "
                       "capturecard.videodevice = \"%5\" ORDER BY %6 %7 "
                       "LIMIT 1;")
                       .arg(channelorder).arg(comp).arg(id)
                       .arg(channelinput).arg(device)
                       .arg(channelorder).arg(ordering);

    query = db_conn->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        ret = query.value(0).toString();
    }
    else
    {
        if (direction)        
            comp = "<";
        else
            comp = ">";

        thequery = QString("SELECT channel.channum FROM channel,capturecard,"
                           "cardinput WHERE channel.%1 %2 \"%3\" AND "
                           "channel.sourceid = cardinput.sourceid AND "
                           "cardinput.inputname = \"%4\" AND "
                           "cardinput.cardid = capturecard.cardid AND "
                           "capturecard.videodevice = \"%5\" ORDER BY %6 %7 "
                           "LIMIT 1;")
                           .arg(channelorder).arg(comp).arg(id)
                           .arg(channelinput).arg(device)
                           .arg(channelorder).arg(ordering);

        query = db_conn->exec(thequery);
 
        if (query.isActive() && query.numRowsAffected() > 0)
        { 
            query.next();

            ret = query.value(0).toString();
        }
    }

    return ret;
}

bool TVRec::ChangeExternalChannel(const QString &channum)
{
    QString command = context->GetSetting("ExternalChannelCommand");

    command += QString(" ") + QString(channum);

    int ret = system(command.ascii());

    if (ret > 0)
        return true;
    return true;
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

void TVRec::SetupRingBuffer(QString &path, long long &filesize, 
                            long long &fillamount, bool pip)
{
    if (pip)
    {
        filesize = context->GetNumSetting("PIPBufferSize");
        fillamount = context->GetNumSetting("PIPMaxBufferFill");
        path = context->GetSetting("PIPBufferName");
    }
    else
    {
        filesize = context->GetNumSetting("BufferSize");
        fillamount = context->GetNumSetting("MaxBufferFill");
        path = context->GetSetting("BufferName");
    }

    filesize = filesize * 1024 * 1024 * 1024;
    fillamount = fillamount * 1024 * 1024;

    rbuffer = new RingBuffer(context, path, filesize, fillamount);
}

void TVRec::SpawnLiveTV(void)
{
    nextState = kState_WatchingLiveTV;
    changeState = true;
}

void TVRec::StopLiveTV(void)
{
    nextState = kState_None;
    changeState = true;
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

void TVRec::ChangeChannel(bool direction)
{
    rbuffer->Reset();
    
    if (direction)
        channel->ChannelUp();
    else
        channel->ChannelDown();

    nvr->Reset();
    nvr->Unpause();

    UnpauseRingBuffer();
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
                        QString &channelname)
{
    GetChannelInfo(channel, title, subtitle, desc, category, starttime,
                   endtime, callsign, iconpath, channelname);
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

    if (whence == SEEK_CUR)
    {
        long long desired = curpos + pos;
        long long realpos = rbuffer->GetReadPosition();

        pos = desired - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    UnpauseRingBuffer();

    return ret;
}

void TVRec::SpawnReadThread(QSocket *sock)
{
    if (readthreadSock)
        return;

    pthread_mutex_init(&readthreadLock, NULL);
    readthreadSock = sock;
    readthreadlive = false;

    pthread_create(&readthread, NULL, ReadThread, this);
}

void TVRec::KillReadThread(void)
{
    readthreadlive = false;
    rbuffer->StopReads();
    pthread_join(readthread, NULL);
}

void TVRec::DoReadThread(void)
{
    readthreadlive = true;

    char *buffer = new char[16001];

    while (readthreadlive)
    {
        pthread_mutex_lock(&readthreadLock);
        int ret = rbuffer->Read(buffer, 16000);
        WriteBlock(readthreadSock, buffer, ret);

        pthread_mutex_unlock(&readthreadLock);
 
        usleep(50);
    }

    delete [] buffer;
}

void *TVRec::ReadThread(void *param)
{
    TVRec *thetv = (TVRec *)param;
    thetv->DoReadThread();

    return NULL;
}

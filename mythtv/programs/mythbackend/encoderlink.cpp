#include <unistd.h>
#include <iostream>

using namespace std;

#include "encoderlink.h"
#include "tv.h"
#include "programinfo.h"

#include "libmyth/mythcontext.h"

EncoderLink::EncoderLink(int capturecardnum, QSocket *lsock, QString lhostname)
{
    sock = lsock;
    hostname = lhostname;
    tv = NULL;
    local = false;
    m_capturecardnum = capturecardnum;
}

EncoderLink::EncoderLink(int capturecardnum, TVRec *ltv)
{
    sock = NULL;
    tv = ltv;
    local = true;
    m_capturecardnum = capturecardnum;
}

EncoderLink::~EncoderLink()
{
    if (local)
        delete tv;
}

bool EncoderLink::isBusy(void)
{
    bool retval = false;

    TVState state = tv->GetState();

    if (state == kState_RecordingOnly || state == kState_WatchingRecording ||
        state == kState_WatchingLiveTV)
    {
        retval = true;
    }

    return retval;
}

bool EncoderLink::isConnected(void)
{
    if (local)
        return true;
       
    if (sock)
        return true;

    return false;
}

// XXX
TVState EncoderLink::GetState(void)
{
    TVState retval = kState_Error;

    if (local)
        retval = tv->GetState();
    else
    {
    }

    return retval;
}

// XXX
bool EncoderLink::MatchesRecording(ProgramInfo *rec)
{
    bool retval = false;

   
    ProgramInfo *tvrec = NULL;

    if (local)
        tvrec = tv->GetRecording();
    

    if (tvrec)
    {
        if (tvrec->chanid == rec->chanid && tvrec->startts == rec->startts &&
            tvrec->endts == rec->endts)
        {
            retval = true;
        }
    }

    return retval;
}

int EncoderLink::AllowRecording(ProgramInfo *rec, int timeuntil)
{
    TVState state = tv->GetState();

    if (state != kState_WatchingLiveTV)
        return 1;

    QString message = QString("MythTV wants to record \"") + rec->title;
    if (gContext->GetNumSetting("DisplayChanNum") != 0)
        message += QString("\" on ") + rec->channame + " [" +
                   rec->chansign + "]";
    else
        message += "\" on Channel " + rec->chanstr;
    message += " in %d seconds.  Do you want to:";

    QString query = QString("ASK_RECORDING %1 %2").arg(m_capturecardnum)
                                                  .arg(timeuntil - 2);

    MythEvent me(query, message);
    gContext->dispatch(me);

    return -1;
}

// XXX
void EncoderLink::StartRecording(ProgramInfo *rec)
{
    if (local)
        tv->StartRecording(rec);
}

// XXX -- make sure this blocks until stopped remotely
void EncoderLink::StopRecording(void)
{
    if (local)
        tv->StopRecording();
}

bool EncoderLink::IsReallyRecording(void)
{
    if (local)
        return tv->IsReallyRecording();

    cerr << "Should be local only query: IsReallyRecording\n";
    return false;
}

float EncoderLink::GetFramerate(void)
{
    if (local)
        return tv->GetFramerate();

    cerr << "Should be local only query: GetFramerate\n";
    return -1;
}

long long EncoderLink::GetFramesWritten(void)
{
    if (local)
        return tv->GetFramesWritten();

    cerr << "Should be local only query: GetFramesWritten\n";
    return -1;
}

long long EncoderLink::GetFilePosition(void)
{
    if (local)
        return tv->GetFilePosition();

    cerr << "Should be local only query: GetFilePosition\n";
    return -1;
}

long long EncoderLink::GetFreeSpace(long long totalreadpos)
{
    if (local)
        return tv->GetFreeSpace(totalreadpos);

    cerr << "Should be local only query: GetFreeSpace\n";
    return -1;
}

long long EncoderLink::GetKeyframePosition(long long desired)
{
    if (local)
        return tv->GetKeyframePosition(desired);

    cerr << "Should be local only query: GetKeyframePosition\n";
    return -1;
}

void EncoderLink::TriggerRecordingTransition(void)
{
    if (local)
    {
        tv->TriggerRecordingTransition();
        return;
    }

    cerr << "Should be local only query: TriggerRecordingTransition\n";
}

void EncoderLink::StopPlaying(void)
{
    if (local)
    {
        tv->StopPlaying();
        return;
    }

    cerr << "Should be local only query: StopPlaying\n";
}

void EncoderLink::SetupRingBuffer(QString &path, long long &filesize,
                                  long long &fillamount, bool pip)
{
    if (local)
    {
        tv->SetupRingBuffer(path, filesize, fillamount, pip);
        return;
    }

    cerr << "Should be local only query: SetupRingBuffer\n";
}

void EncoderLink::SpawnLiveTV(void)
{
    if (local)
    {
        tv->SpawnLiveTV();
        return;
    }

    cerr << "Should be local only query: SpawnLiveTV\n";
}

void EncoderLink::StopLiveTV(void)
{
    if (local)
    {
        tv->StopLiveTV();
        return;
    }

    cerr << "Should be local only query: StopLiveTV\n";
}

void EncoderLink::PauseRecorder(void)
{
    if (local)
    {
        tv->PauseRecorder();
        return;
    }

    cerr << "Should be local only query: PauseRecorder\n";
}

void EncoderLink::ToggleInputs(void)
{
    if (local)
    {
        tv->ToggleInputs();
        return;
    }

    cerr << "Should be local only query: ToggleInputs\n";
}

void EncoderLink::ChangeChannel(bool direction)
{
    if (local)
    {
        tv->ChangeChannel(direction);
        return;
    }

    cerr << "Should be local only query: ChangeChannel\n";
}

void EncoderLink::SetChannel(QString name)
{
    if (local)
    {
        tv->SetChannel(name);
        return;
    }

    cerr << "Should be local only query: SetChannel\n";
}

void EncoderLink::ChangeContrast(bool direction)
{
    if (local)
    {
        tv->ChangeContrast(direction);
        return;
    }

    cerr << "Should be local only query: ChangeContrast\n";
}

void EncoderLink::ChangeBrightness(bool direction)
{
    if (local)
    {
        tv->ChangeBrightness(direction);
        return;
    }

    cerr << "Should be local only query: ChangeBrightness\n";
}

void EncoderLink::ChangeColour(bool direction)
{
    if (local)
    {
        tv->ChangeColour(direction);
        return;
    }

    cerr << "Should be local only query: ChangeColor\n";
}

bool EncoderLink::CheckChannel(QString name)
{
    if (local)
        return tv->CheckChannel(name);

    cerr << "Should be local only query: CheckChannel\n";
    return false;
}

void EncoderLink::GetChannelInfo(QString &title, QString &subtitle, 
                                 QString &desc, QString &category, 
                                 QString &starttime, QString &endtime, 
                                 QString &callsign, QString &iconpath,
                                 QString &channelname)
{
    if (local)
    {
        tv->GetChannelInfo(title, subtitle, desc, category, starttime,
                           endtime, callsign, iconpath, channelname);
        return;
    }

    cerr << "Should be local only query: GetChannelInfo\n";
}

void EncoderLink::GetInputName(QString &inputname)
{
    if (local)
    {
        tv->GetInputName(inputname);
        return;
    }

    cerr << "Should be local only query: GetInputName\n";
}

void EncoderLink::SpawnReadThread(QSocket *sock)
{
    if (local)
    {
        tv->SpawnReadThread(sock);
        return;
    }

    cerr << "Should be local only query: SpawnReadThread\n";
}

void EncoderLink::KillReadThread(void)
{
    if (local)
    {
        tv->KillReadThread();
        return;
    }

    cerr << "Should be local only query: KillReadThread\n";
}

QSocket *EncoderLink::GetReadThreadSocket(void)
{
    if (local)
        return tv->GetReadThreadSocket();

    cerr << "Should be local only query: GetReadThreadSocket\n";
    return NULL;
}

void EncoderLink::PauseRingBuffer(void)
{
    if (local)
    {
        tv->PauseRingBuffer();
        return;
    }

    cerr << "Should be local only query: PauseRingBuffer\n";
}

void EncoderLink::UnpauseRingBuffer(void)
{
    if (local)
    {
        tv->UnpauseRingBuffer();
        return;
    }

    cerr << "Should be local only query: UnpauseRingBuffer\n";
}

void EncoderLink::PauseClearRingBuffer(void)
{
    if (local)
    {
        tv->PauseClearRingBuffer();
        return;
    }

    cerr << "Should be local only query: PauseClearRingBuffer\n";
}

void EncoderLink::RequestRingBufferBlock(int size)
{
    if (local)
    {
        tv->RequestRingBufferBlock(size);
        return;
    }

    cerr << "Should be local only query: RequestRingBufferBlock\n";
}

long long EncoderLink::SeekRingBuffer(long long curpos, long long pos, 
                                      int whence)
{
    if (local)
        return tv->SeekRingBuffer(curpos, pos, whence);

    cerr << "Should be local only query: SeekRingBuffer\n";
    return -1;
}

// XXX
char *EncoderLink::GetScreenGrab(QString filename, int secondsin, 
                                 int &bufferlen, int &video_width, 
                                 int &video_height)
{
    if (local)
        return tv->GetScreenGrab(filename, secondsin, bufferlen, video_width,
                                 video_height);
    return NULL;
}

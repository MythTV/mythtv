#include <qapplication.h>

#include <iostream>
#include <unistd.h>

using namespace std;

#include "remoteencoder.h"
#include "programinfo.h"
#include "util.h"
#include "mythcontext.h"

RemoteEncoder::RemoteEncoder(int num, const QString &host, short port)
{
    recordernum = num;
    remotehost = host;
    remoteport = port;
    backendError = false;

    lastchannel = "";

    controlSock = NULL;
    pthread_mutex_init(&lock, NULL); 
}

RemoteEncoder::~RemoteEncoder()
{
    if (controlSock)
        delete controlSock;
}

void RemoteEncoder::Setup(void)
{
    if (!controlSock)
    {
        controlSock = openControlSocket(remotehost, remoteport);
    }
}

bool RemoteEncoder::IsValidRecorder(void)
{
    return (recordernum >= 0);
}

int RemoteEncoder::GetRecorderNumber(void)
{
    return recordernum;
}

void RemoteEncoder::SendReceiveStringList(QStringList &strlist)
{
    if (!controlSock)
        return;

    pthread_mutex_lock(&lock);

    backendError = false;

    WriteStringList(controlSock, strlist);
    if (!ReadStringList(controlSock, strlist, true))
    {
        cerr << "Remote encoder not responding.\n";
        backendError = true;
    }

    pthread_mutex_unlock(&lock);
}

QSocketDevice *RemoteEncoder::openControlSocket(const QString &host, short port)
{
    QSocketDevice *sock = new QSocketDevice(QSocketDevice::Stream);
    if (!connectSocket(sock, host, port))
    {
        cerr << "Connection timed out.\n";
        delete sock;
        sock = NULL;
    }
    else
    {
        if (gContext->CheckProtoVersion(sock))
        {
            QString hostname = gContext->GetHostName();
            QStringList strlist = QString("ANN Playback %1 %2").arg(hostname).arg(false);
            WriteStringList(sock, strlist);
            ReadStringList(sock, strlist, true);
        }
        else
        {
            delete sock;
            sock = NULL;
        }
    }    
    
    return sock;
}

bool RemoteEncoder::IsRecording(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "IS_RECORDING";

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    return retval;
}

ProgramInfo *RemoteEncoder::GetRecording(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_RECORDING";

    SendReceiveStringList(strlist);

    ProgramInfo *proginfo = new ProgramInfo;
    proginfo->FromStringList(strlist, 0);
    return proginfo;
}

float RemoteEncoder::GetFrameRate(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FRAMERATE";

    SendReceiveStringList(strlist);

    float retval = strlist[0].toFloat();
    return retval;
}

long long RemoteEncoder::GetFramesWritten(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FRAMES_WRITTEN";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long RemoteEncoder::GetFilePosition(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FILE_POSITION";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long RemoteEncoder::GetFreeSpace(long long totalreadpos)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FREE_SPACE";
    encodeLongLong(strlist, totalreadpos);

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

long long RemoteEncoder::GetKeyframePosition(long long desired)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_KEYFRAME_POS";
    encodeLongLong(strlist, desired);

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

void RemoteEncoder::FillPositionMap(int start, int end,
                                    QMap<long long, long long> &positionMap)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "FILL_POSITION_MAP";
    strlist << QString::number(start);
    strlist << QString::number(end);

    SendReceiveStringList(strlist);

    int listpos = 0;
    int listsize = strlist.size(); 

    if (listsize < 4)
        return;

    for(int i = start; listpos < listsize ; i++)
    {
        long long index = decodeLongLong(strlist, listpos);
        positionMap[index] = decodeLongLong(strlist, listpos+2);
        listpos += 4;
    }
}

void RemoteEncoder::CancelNextRecording(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CANCEL_NEXT_RECORDING";
    VERBOSE(VB_IMPORTANT, QString("Sending QUERY_RECORDER %1 - CANCEL_NEXT_RECORDING")
                          .arg(recordernum));
                          
    SendReceiveStringList(strlist);
}

void RemoteEncoder::FrontendReady(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "FRONTEND_READY";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::StopPlaying(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_PLAYING";

    SendReceiveStringList(strlist);
}

bool RemoteEncoder::SetupRingBuffer(QString &path, long long &filesize,
                                    long long &fillamount, bool pip)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SETUP_RING_BUFFER";
    strlist << QString::number((int)pip);

    SendReceiveStringList(strlist);

    bool ok = (strlist[0] == "ok");
    path = strlist[1];

    filesize = decodeLongLong(strlist, 2);
    fillamount = decodeLongLong(strlist, 4);
    return ok;
}

void RemoteEncoder::SpawnLiveTV(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SPAWN_LIVETV";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::StopLiveTV(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_LIVETV";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::Pause(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "PAUSE";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::FinishRecording(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "FINISH_RECORDING";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ToggleInputs(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "TOGGLE_INPUTS";

    SendReceiveStringList(strlist);

    lastchannel = "";
}

void RemoteEncoder::ToggleChannelFavorite(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "TOGGLE_CHANNEL_FAVORITE";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeChannel(int channeldirection)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_CHANNEL";
    strlist << QString::number(channeldirection);

    SendReceiveStringList(strlist);

    lastchannel = "";
}

void RemoteEncoder::SetChannel(QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);

    lastchannel = "";
}

int RemoteEncoder::ChangeContrast(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_CONTRAST";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

int RemoteEncoder::ChangeBrightness(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_BRIGHTNESS";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

int RemoteEncoder::ChangeColour(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_COLOUR";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

int RemoteEncoder::ChangeHue(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_HUE";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

void RemoteEncoder::ChangeDeinterlacer(int deint_mode)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_DEINTERLACER";
    strlist << QString::number((int)deint_mode);

    SendReceiveStringList(strlist);
}

bool RemoteEncoder::CheckChannel(QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHECK_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    return retval;
}

bool RemoteEncoder::CheckChannelPrefix(QString channel, bool &unique) 
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHECK_CHANNEL_PREFIX";
    strlist << channel;

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    unique = strlist[1].toInt();

    return retval;
}

void RemoteEncoder::GetNextProgram(int direction,
                                   QString &title, QString &subtitle,
                                   QString &desc, QString &category,
                                   QString &starttime, QString &endtime,
                                   QString &callsign, QString &iconpath,
                                   QString &channelname, QString &chanid,
                                   QString &seriesid, QString &programid)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_NEXT_PROGRAM_INFO";
    strlist << channelname;
    strlist << chanid;
    strlist << QString::number((int)direction);
    strlist << starttime;

    SendReceiveStringList(strlist);

    title = strlist[0];
    subtitle = strlist[1];
    desc = strlist[2];
    category = strlist[3];
    starttime = strlist[4];
    endtime = strlist[5];
    callsign = strlist[6];
    iconpath = strlist[7];
    channelname = strlist[8];
    chanid = strlist[9];
    seriesid = strlist[10];
    programid = strlist[11];

    if (title == " ")
        title = "";
    if (subtitle == " ")
        subtitle = "";
    if (desc == " ")
        desc = "";
    if (category == " ")
        category = "";
    if (starttime == " ")
        starttime = "";
    if (endtime == " ")
        endtime = "";
    if (callsign == " ")
        callsign = "";
    if (iconpath == " ")
        iconpath = "";
    if (channelname == " ")
        channelname = "";
    if (chanid == " ")
        chanid = "";
    if (seriesid == " ")
        seriesid = "";
    if (programid == " ")
        programid = "";
}

void RemoteEncoder::GetChannelInfo(QString &title, QString &subtitle,
                                   QString &desc, QString &category,
                                   QString &starttime, QString &endtime,
                                   QString &callsign, QString &iconpath,
                                   QString &channelname, QString &chanid,
                                   QString &seriesid, QString &programid,
                                   QString &outputFilters, QString &repeat, 
                                   QString &airdate, QString &stars)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_PROGRAM_INFO";

    SendReceiveStringList(strlist);

    title = strlist[0];
    subtitle = strlist[1];
    desc = strlist[2];
    category = strlist[3];
    starttime = strlist[4];
    endtime = strlist[5];
    callsign = strlist[6];
    iconpath = strlist[7];
    channelname = strlist[8];
    chanid = strlist[9];
    seriesid = strlist[10];
    programid = strlist[11];
    outputFilters = strlist[12];
    
    repeat = strlist[13];
    airdate = strlist[14];
    stars = strlist[15];

    if (title == " ")
        title = "";
    if (subtitle == " ")
        subtitle = "";
    if (desc == " ")
        desc = "";
    if (category == " ")
        category = "";
    if (starttime == " ")
        starttime = "";
    if (endtime == " ")
        endtime = "";
    if (callsign == " ")
        callsign = "";
    if (iconpath == " ")
        iconpath = "";
    if (channelname == " ")
        channelname = "";
    if (chanid == " ")
        chanid = "";
    if (seriesid == " ")
        seriesid = "";
    if (programid == " ")
        programid = "";
    if (outputFilters == " ")
        outputFilters = "";        
    if (repeat == " ")
        repeat = "0";
    if (airdate == " ")
        airdate = "";
    if (stars == " ")
        stars = "";
         
    lastchannel = channelname;
}

void RemoteEncoder::GetInputName(QString &inputname)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_INPUT_NAME";

    SendReceiveStringList(strlist);

    inputname = strlist[0];
}

QString RemoteEncoder::GetCurrentChannel(void)
{
    if (lastchannel == "" || lastchannel == "0")
    {
        QString dummy;
        GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                       dummy, dummy, lastchannel, dummy, dummy, 
                       dummy, dummy, dummy, dummy, dummy);
    }
    return lastchannel;
}

void RemoteEncoder::GetOutputFilters(QString& filters)
{
    QString dummy;
    GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                   dummy, dummy, dummy, dummy, dummy, dummy, filters,
                   dummy, dummy, dummy);
}


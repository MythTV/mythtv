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

/** \fn RemoteEncoder::GetFramerate()
 *  \brief Returns recordering frame rate set by nvr.
 *  \sa TVRec::GetFramerate(), EncoderLink::GetFramerate(void),
 *      RecorderBase::GetFrameRate()
 *  \return Frames per second if query succeeds -1 otherwise.
 */
float RemoteEncoder::GetFrameRate(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FRAMERATE";

    SendReceiveStringList(strlist);

    float retval = strlist[0].toFloat();
    return retval;
}

/** \fn RemoteEncoder::GetFramesWritten()
 *  \brief Returns number of frames written to disk by TVRec's RecorderBase
 *         instance.
 *
 *  \sa TVRec::GetFramesWritten(), EncoderLink::GetFramesWritten()
 *  \return Number of frames if query succeeds, -1 otherwise.
 */
long long RemoteEncoder::GetFramesWritten(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FRAMES_WRITTEN";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

/** \fn RemoteEncoder::GetFilePosition()
 *  \brief Returns total number of bytes written by TVRec's RingBuffer.
 *
 *  \sa TVRec::GetFilePosition(), EncoderLink::GetFilePosition()
 *  \return Bytes written if query succeeds, -1 otherwise.
 */
long long RemoteEncoder::GetFilePosition(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FILE_POSITION";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

/** \fn TVRec::GetFreeSpace(long long)
 *  \brief Returns number of bytes beyond "totalreadpos" it is safe to read.
 *
 *  Note: This may return a negative number, including -1 if the call
 *  succeeds. This means totalreadpos is past the "safe read" portion of
 *  the file.
 *
 *  \sa TVRec::GetFreeSpace(long long), EncoderLink::GetFreeSpace(long long)
 *  \return Returns number of bytes ahead of totalreadpos it is safe to read
 *          if call succeeds, -1 otherwise.
 */
long long RemoteEncoder::GetFreeSpace(long long totalreadpos)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FREE_SPACE";
    encodeLongLong(strlist, totalreadpos);

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

/** \fn TVRec::GetMaxBitrate()
 *   Returns the maximum bits per second this recorder can produce.
 *  \sa TVRec::GetMaxBitrate(), EncoderLink::GetMaxBitrate()
 */
long long RemoteEncoder::GetMaxBitrate()
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_MAX_BITRATE";

    SendReceiveStringList(strlist);

    long long retval = decodeLongLong(strlist, 0);

    return retval;
}

/** \fn RemoteEncoder::GetKeyframePosition(long long)
 *  \brief Returns byte position in RingBuffer of a keyframe.
 *
 *  \sa TVRec::GetKeyframePosition(long long),
 *      EncoderLink::GetKeyframePosition(long long)
 *  \return Byte position of keyframe if query succeeds, -1 otherwise.
 */
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

/** \fn RemoteEncoder::StopPlaying()
 *  \brief Tells TVRec to stop streaming a recording to the frontend.
 *  \sa TVRec::StopPlaying(), EncoderLink::StopPlaying()
 */
void RemoteEncoder::StopPlaying(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_PLAYING";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::SetupRingBuffer(QString&,long long&,long long&,bool)
 *  \brief Sets up TVRec's RingBuffer for "Live TV" playback.
 *
 *  \sa TVRec::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      EncoderLink::SetupRingBuffer(QString&,long long&,long long&,bool),
 *      StopLiveTV()
 *
 *  \param path Returns path to recording.
 *  \param filesize Returns size of recording file in bytes.
 *  \param fillamount Returns the maximum buffer fill in bytes.
 *  \param pip Tells TVRec's RingBuffer that this is for a Picture in Picture display.
 *  \return true if successful, false otherwise.
 */
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

/** \fn TVRec::SpawnLiveTV()
 *  \brief Tells TVRec to Spawn a "Live TV" recorder.
 *  \sa TVRec::SpawnLiveTV(), EncoderLink::SpawnLiveTV()
 */
void RemoteEncoder::SpawnLiveTV(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SPAWN_LIVETV";

    SendReceiveStringList(strlist);
}

/** \fn EncoderLink::StopLiveTV()
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::StopLiveTV(), EncoderLink::StopLiveTV()
 */
void RemoteEncoder::StopLiveTV(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_LIVETV";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::Pause()
 *  \brief Tells TVRec to pause a recorder, used for channel and input changes.
 */
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

/** \fn RemoteEncoder::ChangeContrast(bool)
 *  \brief Changes contrast of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return contrast if it succeeds, -1 otherwise.
 */
int RemoteEncoder::ChangeContrast(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_CONTRAST";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

/** \fn RemoteEncoder::ChangeBrightness(bool)
 *  \brief Changes the brightness of a recording.
 *
 *   Note: In practice this only works with frame grabbing recorders.
 *
 *  \return brightness if it succeeds, -1 otherwise.
 */
int RemoteEncoder::ChangeBrightness(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_BRIGHTNESS";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

/** \fn RemoteEncoder::ChangeColour(bool)
 *  \brief Changes the colour phase of a recording.
 *
 *   Note: In practice this only works with frame grabbing recorders.
 *
 *  \return colour if it succeeds, -1 otherwise.
 */
int RemoteEncoder::ChangeColour(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_COLOUR";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

/** \fn RemoteEncoder::ChangeHue(bool)
 *  \brief Changes the hue of a recording.
 *
 *   Note: In practice this only works with frame grabbing recorders.
 *
 *  \return hue if it succeeds, -1 otherwise.
 */
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

/** \fn RemoteEncoder::CheckChannel(QString)
 *  \brief Checks if named channel exists on current tuner.
 *
 *  \param channel Channel to verify against current tuner.
 *  \return true if it succeeds, false otherwise.
 *  \sa TVRec::CheckChannel(QString),
 *      EncoderLink::CheckChannel(const QString&),
 *      ShouldSwitchToAnotherCard(QString)
 */
bool RemoteEncoder::CheckChannel(QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHECK_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);

    bool retval = strlist[0].toInt();
    return retval;
}

/** \fn EncoderLink::ShouldSwitchToAnotherCard(QString)
 *  \brief Checks if named channel exists on current tuner, or
 *         another tuner.
 *         <b>This only works on local recorders.</b>
 *  \param channelid channel to verify against tuners.
 *  \return true if the channel on another tuner and not current tuner,
 *          false otherwise.
 *  \sa CheckChannel(const QString&)
 */
bool RemoteEncoder::ShouldSwitchToAnotherCard(QString channelid)
{
    // this function returns true if the channelid is not a valid
    // channel on the current recorder. It queries to server in order
    // to determine this.
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SHOULD_SWITCH_CARD";
    strlist << channelid;

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

/** \fn RemoteEncoder::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information about the program that would be seen if we changed
 *         the channel using ChangeChannel(int) with "direction".
 *  \sa TVRec::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      EncoderLink::GetNextProgram(int,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 */
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

/** \fn RemoteEncoder::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&)
 *  \brief Returns information on the current program and current channel.
 *  \sa TVRec::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 *      EncoderLink::GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&),
 */
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

/** \fn EncoderLink::GetInputName(QString&)
 *  \brief Returns the textual name of the current input,
 *         if a tuner is being used.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::GetInputName(QString&), EncoderLink::GetInputName()
 *  \return Returns input name if successful, "" if not.
 */
void RemoteEncoder::GetInputName(QString &inputname)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_INPUT_NAME";

    SendReceiveStringList(strlist);

    inputname = strlist[0];
}

/** \fn RemoteEncoder::GetOutputFilters(QString&)
 *  \brief Returns just the channel name string from GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&).
 */
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

/** \fn RemoteEncoder::GetOutputFilters(QString&)
 *  \brief Returns just the filters string from GetChannelInfo(QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&,QString&).
 */
void RemoteEncoder::GetOutputFilters(QString& filters)
{
    QString dummy;
    GetChannelInfo(dummy, dummy, dummy, dummy, dummy, dummy,
                   dummy, dummy, dummy, dummy, dummy, dummy, filters,
                   dummy, dummy, dummy);
}


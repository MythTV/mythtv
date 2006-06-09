#include <qapplication.h>

#include <unistd.h>

using namespace std;

#include "remoteencoder.h"
#include "programinfo.h"
#include "util.h"
#include "mythcontext.h"
#include "signalmonitor.h"
#include "videooutbase.h"
#include "mythsocket.h"

RemoteEncoder::RemoteEncoder(int num, const QString &host, short port)
    : recordernum(num),       controlSock(NULL),      remotehost(host),
      remoteport(port),       lastchannel(""),        lastinput(""),
      backendError(false),    cachedFramesWritten(0)
{
}

RemoteEncoder::~RemoteEncoder()
{
    if (controlSock)
        controlSock->DownRef();
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
    QMutexLocker locker(&lock);
    if (!controlSock)
        return;

    backendError = false;

    controlSock->writeStringList(strlist);
    if (!controlSock->readStringList(strlist, true))
    {
        VERBOSE(VB_IMPORTANT,
                "RemoteEncoder::SendReceiveStringList(): No response.");
        backendError = true;
    }
}

MythSocket *RemoteEncoder::openControlSocket(const QString &host, short port)
{
    MythSocket *sock = new MythSocket();
    if (!sock->connect(host, port))
    {
        VERBOSE(VB_IMPORTANT,
                "RemoteEncoder::openControlSocket(): Connection timed out.");
        sock->DownRef();
        sock = NULL;
    }
    else
    {
        if (gContext->CheckProtoVersion(sock))
        {
            QString hostname = gContext->GetHostName();
            QStringList strlist = QString("ANN Playback %1 %2").arg(hostname).arg(false);
            sock->writeStringList(strlist);
            sock->readStringList(strlist, true);
        }
        else
        {
            sock->DownRef();
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

/** \fn RemoteEncoder::GetFrameRate(void)
 *  \brief Returns recordering frame rate set by nvr.
 *  \sa TVRec::GetFramerate(void), EncoderLink::GetFramerate(void),
 *      RecorderBase::GetFrameRate(void)
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

/** \fn RemoteEncoder::GetFramesWritten(void)
 *  \brief Returns number of frames written to disk by TVRec's RecorderBase
 *         instance.
 *
 *  \sa TVRec::GetFramesWritten(void), EncoderLink::GetFramesWritten(void)
 *  \return Number of frames if query succeeds, -1 otherwise.
 */
long long RemoteEncoder::GetFramesWritten(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_FRAMES_WRITTEN";

    SendReceiveStringList(strlist);

    cachedFramesWritten = decodeLongLong(strlist, 0);

    return cachedFramesWritten;
}

/** \fn RemoteEncoder::GetFilePosition(void)
 *  \brief Returns total number of bytes written by TVRec's RingBuffer.
 *
 *  \sa TVRec::GetFilePosition(void), EncoderLink::GetFilePosition(void)
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

/** \fn TVRec::GetMaxBitrate(void)
 *   Returns the maximum bits per second this recorder can produce.
 *  \sa TVRec::GetMaxBitrate(void), EncoderLink::GetMaxBitrate(void)
 */
long long RemoteEncoder::GetMaxBitrate(void)
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

void RemoteEncoder::CancelNextRecording(bool cancel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CANCEL_NEXT_RECORDING";
    strlist << QString::number((cancel) ? 1 : 0);
                          
    SendReceiveStringList(strlist);
}

void RemoteEncoder::FrontendReady(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "FRONTEND_READY";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::StopPlaying(void)
 *  \brief Tells TVRec to stop streaming a recording to the frontend.
 *  \sa TVRec::StopPlaying(void), EncoderLink::StopPlaying(void)
 */
void RemoteEncoder::StopPlaying(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_PLAYING";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::SpawnLiveTV(QString,bool)
 *  \brief Tells TVRec to Spawn a "Live TV" recorder.
 *  \sa TVRec::SpawnLiveTV(LiveTVChain*,bool),
 *      EncoderLink::SpawnLiveTV(LiveTVChain*,bool)
 */
void RemoteEncoder::SpawnLiveTV(QString chainId, bool pip)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SPAWN_LIVETV";
    strlist << chainId;
    strlist << QString::number((int)pip);

    SendReceiveStringList(strlist);
}

/** \fn EncoderLink::StopLiveTV(void)
 *  \brief Tells TVRec to stop a "Live TV" recorder.
 *         <b>This only works on local recorders.</b>
 *  \sa TVRec::StopLiveTV(void), EncoderLink::StopLiveTV(void)
 */
void RemoteEncoder::StopLiveTV(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "STOP_LIVETV";

    SendReceiveStringList(strlist);
}

/** \fn RemoteEncoder::PauseRecorder(void)
 *  \brief Tells TVRec to pause a recorder, used for channel and input changes.
 *  \sa TVRec::PauseRecorder(void), EncoderLink::PauseRecorder(void),
 *      RecorderBase::Pause(void)
 */
void RemoteEncoder::PauseRecorder(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "PAUSE";

    SendReceiveStringList(strlist);

    lastinput = "";
}

void RemoteEncoder::FinishRecording(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "FINISH_RECORDING";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::SetLiveRecording(bool recording)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_LIVE_RECORDING";
    strlist << QString::number(recording);

    SendReceiveStringList(strlist);
}

QStringList RemoteEncoder::GetInputs(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_CONNECTED_INPUTS";

    SendReceiveStringList(strlist);

    return strlist;
}

QString RemoteEncoder::GetInput(void)
{
    if (lastinput.length() > 2)
        return lastinput;

    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_INPUT";

    SendReceiveStringList(strlist);

    lastinput = strlist[0];
    return lastinput; 
}

QString RemoteEncoder::SetInput(QString input)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_INPUT";
    strlist << input;

    SendReceiveStringList(strlist);

    lastchannel = "";
    lastinput = "";

    return strlist[0];
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
    lastinput = "";
}

void RemoteEncoder::SetChannel(QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);

    lastchannel = "";
    lastinput = "";
}

/** \fn RemoteEncoder::SetSignalMonitoringRate(int,bool)
 *  \brief Sets the signal monitoring rate.
 *
 *  This will actually call SetupSignalMonitor() and 
 *  TeardownSignalMonitor(bool) as needed, so it can
 *  be used directly, without worrying about the 
 *  SignalMonitor instance.
 *
 *  \sa TVRec::SetSignalMonitoringRate(int,int),
 *      EncoderLink::SetSignalMonitoringRate(int,bool)
 *  \param rate           The update rate to use in milliseconds,
 *                        use 0 to disable.
 *  \param notifyFrontend If true, SIGNAL messages will be sent to
 *                        the frontend using this recorder.
 *  \return Previous update rate
 */
int RemoteEncoder::SetSignalMonitoringRate(int rate, bool notifyFrontend)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_SIGNAL_MONITORING_RATE";
    strlist << QString::number(rate);
    strlist << QString::number((int)notifyFrontend);

    SendReceiveStringList(strlist);
  
    int retval = strlist[0].toInt();
    return retval;
}

uint RemoteEncoder::GetSignalLockTimeout(QString input)
{
    QMutexLocker locker(&lock);

    QMap<QString,uint>::const_iterator it = cachedTimeout.find(input);
    if (it != cachedTimeout.end())
        return *it;

    uint cardid  = recordernum;
    uint timeout = 0xffffffff;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT channel_timeout, cardtype "
        "FROM cardinput, capturecard "
        "WHERE cardinput.inputname = :INNAME AND "
        "      cardinput.cardid    = :CARDID AND "
        "      ( ( cardinput.childcardid =  '0' AND "
        "          cardinput.cardid = capturecard.cardid) OR "
        "        ( cardinput.childcardid != '0' AND "
        "          cardinput.childcardid = capturecard.cardid) )");
    query.bindValue(":INNAME", input);
    query.bindValue(":CARDID", cardid);
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting timeout", query);
    else if (query.next() &&
             SignalMonitor::IsSupported(query.value(1).toString()))
        timeout = max(query.value(0).toInt(), 500);

/*
    VERBOSE(VB_PLAYBACK, "RemoteEncoder: " +
            QString("GetSignalLockTimeout(%1): Set lock timeout to %2 ms")
            .arg(cardid).arg(timeout));
*/
    cachedTimeout[input] = timeout;
    return timeout;
}


int RemoteEncoder::GetPictureAttribute(int attr)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);

    if (kPictureAttribute_Contrast == attr)
        strlist << "GET_CONTRAST";
    else if (kPictureAttribute_Brightness == attr)
        strlist << "GET_BRIGHTNESS";
    else if (kPictureAttribute_Colour == attr)
        strlist << "GET_COLOUR";
    else if (kPictureAttribute_Hue == attr)
        strlist << "GET_HUE";
    else
        return -1;

    SendReceiveStringList(strlist);

    int retval = strlist[0].toInt();
    return retval;
}

/** \fn RemoteEncoder::ChangeContrast(int, int, bool)
 *  \brief Changes brightness/contrast/colour/hue of a recording.
 *
 *  Note: In practice this only works with frame grabbing recorders.
 *
 *  \return contrast if it succeeds, -1 otherwise.
 */
int RemoteEncoder::ChangePictureAttribute(int type, int attr, bool up)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);

    if (kPictureAttribute_Contrast == attr)
        strlist << "CHANGE_CONTRAST";
    else if (kPictureAttribute_Brightness == attr)
        strlist << "CHANGE_BRIGHTNESS";
    else if (kPictureAttribute_Colour == attr)
        strlist << "CHANGE_COLOUR";
    else if (kPictureAttribute_Hue == attr)
        strlist << "CHANGE_HUE";
    else
        return -1;

    strlist << QString::number(type);
    strlist << QString::number((int)up);

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

/** \fn RemoteEncoder::ShouldSwitchToAnotherCard(QString)
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

/** \fn RemoteEncoder::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *  \brief Checks a prefix against the channels in the DB.
 *
 *  \sa TVRec::CheckChannelPrefix(const QString&,uint&,bool&,QString&)
 *      for details.
 */
bool RemoteEncoder::CheckChannelPrefix(
    const QString &prefix,
    uint          &is_complete_valid_channel_on_rec,
    bool          &is_extra_char_useful,
    QString       &needed_spacer)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHECK_CHANNEL_PREFIX";
    strlist << prefix;

    SendReceiveStringList(strlist);

    is_complete_valid_channel_on_rec = strlist[1].toInt();
    is_extra_char_useful = strlist[2].toInt();
    needed_spacer = (strlist[3] == "X") ? "" : strlist[3];

    return strlist[0].toInt();
}

static QString cleanup(const QString &str)
{
    if (str == " ")
        return "";
    return str;
}

static QString make_safe(const QString &str)
{
    if (str.isEmpty())
        return " ";
    return str;
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

    title       = cleanup(strlist[0]);
    subtitle    = cleanup(strlist[1]);
    desc        = cleanup(strlist[2]);
    category    = cleanup(strlist[3]);
    starttime   = cleanup(strlist[4]);
    endtime     = cleanup(strlist[5]);
    callsign    = cleanup(strlist[6]);
    iconpath    = cleanup(strlist[7]);
    channelname = cleanup(strlist[8]);
    chanid      = cleanup(strlist[9]);
    seriesid    = cleanup(strlist[10]);
    programid   = cleanup(strlist[11]);
}

void RemoteEncoder::GetChannelInfo(QMap<QString, QString> &infoMap,
                                   uint chanid)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_CHANNEL_INFO";
    strlist << QString::number(chanid);

    SendReceiveStringList(strlist);

    infoMap["chanid"]   = cleanup(strlist[0]);
    infoMap["sourceid"] = cleanup(strlist[1]);
    infoMap["callsign"] = cleanup(strlist[2]);
    infoMap["channum"]  = cleanup(strlist[3]);
    infoMap["channame"] = cleanup(strlist[4]);
    infoMap["XMLTV"]    = cleanup(strlist[5]);

    infoMap["oldchannum"] =  infoMap["channum"];
}

bool RemoteEncoder::SetChannelInfo(const QMap<QString, QString> &infoMap)
{
    QStringList strlist = "SET_CHANNEL_INFO";
    strlist << make_safe(infoMap["chanid"]);
    strlist << make_safe(infoMap["sourceid"]);
    strlist << make_safe(infoMap["oldchannum"]);
    strlist << make_safe(infoMap["callsign"]);
    strlist << make_safe(infoMap["channum"]);
    strlist << make_safe(infoMap["channame"]);
    strlist << make_safe(infoMap["XMLTV"]);

    SendReceiveStringList(strlist);

    return strlist[0].toInt();
}

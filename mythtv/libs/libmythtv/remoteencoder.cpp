#include <iostream>
#include <unistd.h>

using namespace std;

#include "remoteencoder.h"
#include "programinfo.h"
#include "util.h"

RemoteEncoder::RemoteEncoder(int num, const QString &host, short port)
{
    recordernum = num;
    remotehost = host;
    remoteport = port;

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

    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);

    pthread_mutex_unlock(&lock);
}

QSocket *RemoteEncoder::openControlSocket(const QString &host, short port)
{
    QSocket *sock = new QSocket();

    sock->connectToHost(host, port);

    int num = 0;
    while (sock->state() == QSocket::HostLookup ||
           sock->state() == QSocket::Connecting)
    {
        usleep(50);
        num++;
        if (num > 100)
        {
            cerr << "Connection timed out.\n";
            exit(0);
        }
    }

    if (sock->state() != QSocket::Connected)
    {
        cout << "Could not connect to server\n";
        return NULL;
    }

    char localhostname[256];
    if (gethostname(localhostname, 256))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }

    QStringList strlist;

    strlist = QString("ANN Playback %1 %2").arg(localhostname).arg(false);
    WriteStringList(sock, strlist);
    ReadStringList(sock, strlist);

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

void RemoteEncoder::SetupRingBuffer(QString &path, long long &filesize,
                                    long long &fillamount, bool pip)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SETUP_RING_BUFFER";
    strlist << QString::number((int)pip);

    SendReceiveStringList(strlist);

    path = strlist[0];

    filesize = decodeLongLong(strlist, 1);
    fillamount = decodeLongLong(strlist, 3);
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

void RemoteEncoder::ToggleInputs(void)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "TOGGLE_INPUTS";

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeChannel(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_CHANNEL";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);
}

void RemoteEncoder::SetChannel(QString channel)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "SET_CHANNEL";
    strlist << channel;

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeContrast(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_CONTRAST";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeBrightness(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_BRIGHTNESS";
    strlist << QString::number((int)direction);

    SendReceiveStringList(strlist);
}

void RemoteEncoder::ChangeColour(bool direction)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "CHANGE_COLOUR";
    strlist << QString::number((int)direction);

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

void RemoteEncoder::GetChannelInfo(QString &title, QString &subtitle,
                                   QString &desc, QString &category,
                                   QString &starttime, QString &endtime,
                                   QString &callsign, QString &iconpath,
                                   QString &channelname)
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
}

void RemoteEncoder::GetInputName(QString &inputname)
{
    QStringList strlist = QString("QUERY_RECORDER %1").arg(recordernum);
    strlist << "GET_INPUT_NAME";

    SendReceiveStringList(strlist);

    inputname = strlist[0];
}


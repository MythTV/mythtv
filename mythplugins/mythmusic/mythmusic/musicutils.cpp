// c/c++
#include <iostream>

// qt
#include <QFile>
#include <QRegExp>

// mythtv
#include <mythdirs.h>
#include "mythlogging.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// mythmusic
#include "musicutils.h"

static QRegExp badChars = QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)");

QString fixFilename(const QString &filename)
{
    QString ret = filename;
    ret.replace(badChars, "_");
    return ret;
}

QString findIcon(const QString &type, const QString &name)
{
    QString cleanName = fixFilename(name);
    QString file = GetConfDir() + QString("/MythMusic/Icons/%1/%2").arg(type).arg(cleanName);

    if (QFile::exists(file + ".jpg"))
        return file + ".jpg";

    if (QFile::exists(file + ".jpeg"))
        return file + ".jpeg";

    if (QFile::exists(file + ".png"))
        return file + ".png";

    if (QFile::exists(file + ".gif"))
        return file + ".gif";

    return QString();
}

uint calcTrackLength(const QString &musicFile)
{
    LOG(VB_GENERAL, LOG_INFO,  "**calcTrackLength - start");
    const char *type = NULL;

//    av_register_all();

    AVFormatContext *inputFC = NULL;
    AVInputFormat *fmt = NULL;

    if (type)
        fmt = av_find_input_format(type);

    // Open recording
    LOG(VB_GENERAL, LOG_DEBUG, QString("calcTrackLength: Opening '%1'")
            .arg(musicFile));

    QByteArray inFileBA = musicFile.toLocal8Bit();

    int ret = av_open_input_file(&inputFC, inFileBA.constData(), fmt, 0, NULL);

    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, "calcTrackLength: Couldn't open input file" +
                                  ENO);
        return 0;
    }

    // Getting stream information
    ret = av_find_stream_info(inputFC);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("calcTrackLength: Couldn't get stream info, error #%1").arg(ret));
        av_close_input_file(inputFC);
        inputFC = NULL;
        return 0;
    }

    uint duration = 0;
    long long time = 0;

    for (uint i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        char buf[256];

        avcodec_string(buf, sizeof(buf), st->codec, false);

        switch (inputFC->streams[i]->codec->codec_type)
        {
            case CODEC_TYPE_AUDIO:
            {
                AVPacket pkt;
                av_init_packet(&pkt);

                while (av_read_frame(inputFC, &pkt) >= 0)
                {
                    if (pkt.stream_index == (int)i)
                        time = time + pkt.duration;

                    av_free_packet(&pkt);
                }

                duration = time * av_q2d(inputFC->streams[i]->time_base);
                break;
            }

            default:
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Skipping unsupported codec %1 on stream %2")
                        .arg(inputFC->streams[i]->codec->codec_type).arg(i));
                break;
        }
    }

    // Close input file
    av_close_input_file(inputFC);
    inputFC = NULL;

    LOG(VB_GENERAL, LOG_INFO,  "**calcTrackLength - end");

    return duration;
}

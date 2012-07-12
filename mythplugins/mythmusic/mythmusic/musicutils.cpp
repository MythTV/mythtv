// c/c++
#include <iostream>

// qt
#include <QFile>
#include <QRegExp>
#include <QDir>

// mythtv
#include <mythdirs.h>
#include <mythlogging.h>
#include <mythcorecontext.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// mythmusic
#include "metadata.h"
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

//TODO this needs updating to also use storage groups
uint calcTrackLength(const QString &musicFile)
{
    const char *type = NULL;

    AVFormatContext *inputFC = NULL;
    AVInputFormat *fmt = NULL;

//     if (type)
//         fmt = av_find_input_format(type);

    // Open recording
    LOG(VB_GENERAL, LOG_DEBUG, QString("calcTrackLength: Opening '%1'")
            .arg(musicFile));

    QByteArray inFileBA = musicFile.toLocal8Bit();

    int ret = avformat_open_input(&inputFC, inFileBA.constData(), fmt, NULL);

    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, "calcTrackLength: Couldn't open input file" +
                                  ENO);
        return 0;
    }

    // Getting stream information
    ret = avformat_find_stream_info(inputFC, NULL);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("calcTrackLength: Couldn't get stream info, error #%1").arg(ret));
        avformat_close_input(&inputFC);
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
            case AVMEDIA_TYPE_AUDIO:
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
    avformat_close_input(&inputFC);
    inputFC = NULL;

    return duration;
}

inline QString fixFileToken_sl(QString token)
{
    // this version doesn't remove fwd-slashes so we can
    // pick them up later and create directories as required
    token.replace(QRegExp("(\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}

QString filenameFromMetadata(Metadata *track, bool createDir)
{
    QDir directoryQD(gMusicData->musicDir);
    QString filename;
    QString fntempl = gCoreContext->GetSetting("FilenameTemplate");
    bool no_ws = gCoreContext->GetNumSetting("NoWhitespace", 0);

    QRegExp rx_ws("\\s{1,}");
    QRegExp rx("(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)");
    int i = 0;
    int old_i = 0;
    while (i >= 0)
    {
        i = rx.indexIn(fntempl, i);
        if (i >= 0)
        {
            if (i > 0)
                filename += fixFileToken_sl(fntempl.mid(old_i,i-old_i));
            i += rx.matchedLength();
            old_i = i;

            if ((rx.capturedTexts()[1] == "GENRE") && (!track->Genre().isEmpty()))
                filename += fixFilename(track->Genre());

            if ((rx.capturedTexts()[1] == "ARTIST")
                    && (!track->FormatArtist().isEmpty()))
                filename += fixFilename(track->FormatArtist());

            if ((rx.capturedTexts()[1] == "ALBUM") && (!track->Album().isEmpty()))
                filename += fixFilename(track->Album());

            if ((rx.capturedTexts()[1] == "TRACK") && (track->Track() >= 0))
            {
                QString tempstr = QString::number(track->Track(), 10);
                if (track->Track() < 10)
                    tempstr.prepend('0');
                filename += fixFilename(tempstr);
            }

            if ((rx.capturedTexts()[1] == "TITLE")
                    && (!track->FormatTitle().isEmpty()))
                filename += fixFilename(track->FormatTitle());

            if ((rx.capturedTexts()[1] == "YEAR") && (track->Year() >= 0))
                filename += fixFilename(QString::number(track->Year(), 10));
        }
    }

    if (no_ws)
        filename.replace(rx_ws, "_");


    if (filename == "" || filename.length() > FILENAME_MAX)
    {
        QString tempstr = QString::number(track->Track(), 10);
        tempstr += " - " + track->FormatTitle();
        filename = fixFilename(tempstr);
        LOG(VB_GENERAL, LOG_ERR, "Invalid file storage definition.");
    }

    if (createDir)
    {
        QFileInfo fi(filename);
        if (!directoryQD.mkpath(gMusicData->musicDir + fi.path()))
            LOG(VB_GENERAL, LOG_ERR,
                QString("Ripper: Failed to create directory path: '%1'").arg(gMusicData->musicDir + filename));
    }

    return filename;
}

bool isNewTune(const QString& artist, const QString& album, const QString& title)
{

    QString matchartist = artist;
    QString matchalbum = album;
    QString matchtitle = title;

    if (! matchartist.isEmpty())
    {
        matchartist.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    if (! matchalbum.isEmpty())
    {
        matchalbum.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    if (! matchtitle.isEmpty())
    {
        matchtitle.replace(QRegExp("(/|\\\\|:|\'|\\,|\\!|\\(|\\)|\"|\\?|\\|)"), QString("_"));
    }

    MSqlQuery query(MSqlQuery::InitCon());
    QString queryString("SELECT filename, artist_name,"
                        " album_name, name, song_id "
                        "FROM music_songs "
                        "LEFT JOIN music_artists"
                        " ON music_songs.artist_id=music_artists.artist_id "
                        "LEFT JOIN music_albums"
                        " ON music_songs.album_id=music_albums.album_id "
                        "WHERE artist_name LIKE :ARTIST "
                        "AND album_name LIKE :ALBUM "
                        "AND name LIKE :TITLE "
                        "ORDER BY artist_name, album_name,"
                        " name, song_id, filename");

    query.prepare(queryString);

    query.bindValue(":ARTIST", matchartist);
    query.bindValue(":ALBUM", matchalbum);
    query.bindValue(":TITLE", matchtitle);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("Search music database", query);
        return true;
    }

    if (query.size() > 0)
        return false;

    return true;
}

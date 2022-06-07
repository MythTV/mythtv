// c/c++
#include <iostream>

// qt
#include <QFile>
#include <QRegularExpression>
#include <QDir>

// mythtv
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/remotefile.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// libmythmetadata
#include "musicmetadata.h"
#include "musicutils.h"

const static QRegularExpression badChars1 { R"((/|\\|:|'|"|\?|\|))" };
const static QRegularExpression badChars2 { R"((/|\\|:|'|\,|\!|\(|\)|"|\?|\|))" };

QString fixFilename(const QString &filename)
{
    QString ret = filename;
    return ret.replace(badChars1, "_");
}

static QMap<QString, QString> iconMap;
QString findIcon(const QString &type, const QString &name, bool ignoreCache)
{
    LOG(VB_FILE, LOG_INFO, QString("findicon: looking for type: %1, name: %2").arg(type, name));

    if (!ignoreCache)
    {
        QMap<QString, QString>::iterator i = iconMap.find(type + name);
        if (i != iconMap.end())
        {
            LOG(VB_FILE, LOG_INFO, QString("findicon: found in cache %1").arg(i.value()));
            return i.value();
        }
    }

    QString cleanName = fixFilename(name) + '.';
    cleanName = '^' + QRegularExpression::escape(cleanName);
    QString file = QString("/Icons/%1/%2").arg(type, cleanName);
    QString imageExtensions = "(jpg|jpeg|png|gif)";
    QStringList fileList;

    fileList = RemoteFile::FindFileList(file + imageExtensions, gCoreContext->GetMasterHostName(), "MusicArt", true, true);
    if (!fileList.isEmpty())
    {
        LOG(VB_FILE, LOG_INFO, QString("findicon: found %1 icons using %2").arg(fileList.size()).arg(fileList[0]));
        iconMap.insert(type + name, fileList[0]);
        return fileList[0];
    }

    iconMap.insert(type + name, QString());

    LOG(VB_FILE, LOG_INFO, QString("findicon: not found type: %1, name: %2").arg(type, name));

    return {};
}

QString fixFileToken_sl(QString token)
{
    // this version doesn't remove fwd-slashes so we can
    // pick them up later and create directories as required
    static const QRegularExpression kBadFilenameCharRE { R"((\\|:|'|"|\?|\|))" };
    token.replace(kBadFilenameCharRE, QString("_"));
    return token;
}

QString filenameFromMetadata(MusicMetadata *track)
{
    QString filename;
    QString fntempl = gCoreContext->GetSetting("FilenameTemplate");
    bool no_ws = gCoreContext->GetBoolSetting("NoWhitespace", false);

    static const QRegularExpression rx_ws("\\s{1,}");
    static const QRegularExpression rx("^(.*?)(GENRE|ARTIST|ALBUM|TRACK|TITLE|YEAR)");
    auto match = rx.match(fntempl);
    while (match.hasMatch())
    {
        filename += match.captured(1);

        if ((match.captured(2) == "GENRE") &&
            (!track->Genre().isEmpty()))
            filename += fixFilename(track->Genre());
        else if ((match.captured(2) == "ARTIST") &&
                 (!track->FormatArtist().isEmpty()))
            filename += fixFilename(track->FormatArtist());
        else if ((match.captured(2) == "ALBUM") &&
                 (!track->Album().isEmpty()))
            filename += fixFilename(track->Album());
        else if ((match.captured(2) == "TRACK") && (track->Track() >= 0))
            filename += fixFilename(QString("%1").arg(track->Track(), 2,10,QChar('0')));
        else if ((match.captured(2) == "TITLE") &&
                 (!track->FormatTitle().isEmpty()))
            filename += fixFilename(track->FormatTitle());
        else if ((match.captured(2) == "YEAR") && (track->Year() >= 0))
            filename += fixFilename(QString::number(track->Year(), 10));
        fntempl.remove(0, match.capturedLength());
        match = rx.match(fntempl);
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

    return filename;
}

bool isNewTune(const QString& artist, const QString& album, const QString& title)
{

    QString matchartist = artist;
    QString matchalbum = album;
    QString matchtitle = title;

    if (! matchartist.isEmpty())
        matchartist.replace(badChars2, QString("_"));

    if (! matchalbum.isEmpty())
        matchalbum.replace(badChars2, QString("_"));

    if (! matchtitle.isEmpty())
        matchtitle.replace(badChars2, QString("_"));

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

    return query.size() <= 0;
}

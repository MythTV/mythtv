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
#include <remotefile.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// libmythmetadata
#include "musicmetadata.h"
#include "musicutils.h"

static QRegExp badChars = QRegExp("(/|\\\\|:|\'|\"|\\?|\\|)");

QString fixFilename(const QString &filename)
{
    QString ret = filename;
    ret.replace(badChars, "_");
    return ret;
}

static QMap<QString, QString> iconMap;
QString findIcon(const QString &type, const QString &name, bool ignoreCache)
{
    if (!ignoreCache)
    {
        QMap<QString, QString>::iterator i = iconMap.find(type + name);
        if (i != iconMap.end())
            return i.value();
    }

    QString cleanName = fixFilename(name);
    QString file = QString("Icons/%1/%2").arg(type).arg(cleanName);
    QStringList imageExtensions = QStringList() << ".jpg" << ".jpeg" << ".png" << ".gif";

    // TODO also look on any slave BEs?
    QString filename = gCoreContext->GenMythURL(gCoreContext->GetMasterHostName(),
                                                0, file, "MusicArt");

    for (int x = 0; x < imageExtensions.count(); x++)
    {
        if (RemoteFile::Exists(filename + imageExtensions[x]))
        {
            iconMap.insert(type + name, filename + imageExtensions[x]);
            return filename + imageExtensions[x];
        }
    }

    iconMap.insert(type + name, QString());

    LOG(VB_FILE, LOG_INFO, QString("findicon: not found for type: %1, name: %2").arg(type).arg(name));

    return QString();
}

inline QString fixFileToken_sl(QString token)
{
    // this version doesn't remove fwd-slashes so we can
    // pick them up later and create directories as required
    token.replace(QRegExp("(\\\\|:|\'|\"|\\?|\\|)"), QString("_"));
    return token;
}

QString filenameFromMetadata(MusicMetadata *track)
{
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

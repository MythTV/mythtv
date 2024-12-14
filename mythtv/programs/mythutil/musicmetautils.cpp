// qt
#include <QDir>
#include <QDomDocument>
#include <QProcess>

// libmyth* headers
#include "libmyth/mythcontext.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/storagegroup.h"
#include "libmythmetadata/metaio.h"
#include "libmythmetadata/musicfilescanner.h"
#include "libmythmetadata/musicmetadata.h"
#include "libmythmetadata/musicutils.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// mythutils headers
#include "musicmetautils.h"
#include "mythutil_commandlineparser.h"

static int UpdateMeta(const MythUtilCommandLineParser &cmdline)
{
    bool ok = true;
    int result = GENERIC_EXIT_OK;

    if (cmdline.toString("songid").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --songid option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    int songID = cmdline.toInt("songid");

    MusicMetadata *mdata = MusicMetadata::createFromID(songID);
    if (!mdata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find metadata for trackid: %1").arg(songID));
        return GENERIC_EXIT_NOT_OK;
    }

    if (!cmdline.toString("title").isEmpty())
        mdata->setTitle(cmdline.toString("title"));

    if (!cmdline.toString("artist").isEmpty())
        mdata->setArtist(cmdline.toString("artist"));

    if (!cmdline.toString("album").isEmpty())
        mdata->setAlbum(cmdline.toString("album"));

    if (!cmdline.toString("genre").isEmpty())
        mdata->setGenre(cmdline.toString("genre"));

    if (!cmdline.toString("trackno").isEmpty())
        mdata->setTrack(cmdline.toInt("trackno"));

    if (!cmdline.toString("year").isEmpty())
        mdata->setYear(cmdline.toInt("year"));

    if (!cmdline.toString("rating").isEmpty())
        mdata->setRating(cmdline.toInt("rating"));

    if (!cmdline.toString("playcount").isEmpty())
        mdata->setPlaycount(cmdline.toInt("playcount"));

    if (!cmdline.toString("lastplayed").isEmpty())
        mdata->setLastPlay(cmdline.toDateTime("lastplayed"));

    mdata->dumpToDatabase();

    MetaIO *tagger = mdata->getTagger();
    if (tagger)
    {
        ok = tagger->write(mdata->getLocalFilename(), mdata);

        if (!ok)
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to write to tag for trackid: %1").arg(songID));
    }

    // tell any clients that the metadata for this track has changed
    gCoreContext->SendMessage(QString("MUSIC_METADATA_CHANGED %1").arg(songID));

    if (!ok)
        result = GENERIC_EXIT_NOT_OK;

    return result;
}

static int ExtractImage(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("songid").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --songid option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toString("imagetype").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --imagetype option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    int songID = cmdline.toInt("songid");
    ImageType type = (ImageType)cmdline.toInt("imagetype");

    MusicMetadata *mdata = MusicMetadata::createFromID(songID);
    if (!mdata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find metadata for trackid: %1").arg(songID));
        return GENERIC_EXIT_NOT_OK;
    }

    AlbumArtImage *image = mdata->getAlbumArtImages()->getImage(type);
    if (!image)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find image of type: %1").arg(type));
        return GENERIC_EXIT_NOT_OK;
    }

    MetaIO *tagger = mdata->getTagger();
    if (!tagger)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find a tagger for this file: %1").arg(mdata->Filename(false)));
        return GENERIC_EXIT_NOT_OK;
    }


    if (!image->m_embedded || !tagger->supportsEmbeddedImages())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Either the image isn't embedded or the tagger doesn't support embedded images"));
        delete tagger;
        return GENERIC_EXIT_NOT_OK;
    }

    // find the tracks actual filename
    StorageGroup musicGroup("Music", gCoreContext->GetHostName(), false);
    QString trackFilename =  musicGroup.FindFile(mdata->Filename(false));

    // where are we going to save the image
    QString path;
    StorageGroup artGroup("MusicArt", gCoreContext->GetHostName(), false);
    QStringList dirList = artGroup.GetDirList();
    if (!dirList.empty())
        path = artGroup.FindNextDirMostFree();

    if (!QDir(path).exists())
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot find a directory in the 'MusicArt' storage group to save to");
        delete tagger;
        return GENERIC_EXIT_NOT_OK;
    }

    path += "/AlbumArt/";
    QDir dir(path);

    QString filename = QString("%1-%2.jpg").arg(mdata->ID()).arg(AlbumArtImages::getTypeFilename(image->m_imageType));

    if (QFile::exists(path + filename))
        QFile::remove(path + filename);

    if (!dir.exists())
        dir.mkpath(path);

    QImage *saveImage = tagger->getAlbumArt(trackFilename, image->m_imageType);
    if (saveImage)
    {
        saveImage->save(path + filename, "JPEG");
        delete saveImage;
    }

    delete tagger;

    // tell any clients that the albumart for this track has changed
    gCoreContext->SendMessage(QString("MUSIC_ALBUMART_CHANGED %1 %2").arg(songID).arg(type));

    return GENERIC_EXIT_OK;
}

static int ScanMusic(const MythUtilCommandLineParser &cmdline)
{
    auto *fscan = new MusicFileScanner(cmdline.toBool("musicforce"));
    QStringList dirList;

    if (!StorageGroup::FindDirs("Music", gCoreContext->GetHostName(), &dirList))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find any directories in the 'Music' storage group");
        delete fscan;
        return GENERIC_EXIT_NOT_OK;
    }

    fscan->SearchDirs(dirList);
    delete fscan;

    return GENERIC_EXIT_OK;
}

static int UpdateRadioStreams(const MythUtilCommandLineParser &/*cmdline*/)
{
    // check we have the correct Music Schema Version (maybe the FE hasn't been run yet)
    if (gCoreContext->GetNumSetting("MusicDBSchemaVer", 0) < 1024)
    {
        LOG(VB_GENERAL, LOG_ERR, "Can't update the radio streams the DB schema hasn't been updated yet! Aborting");
        return GENERIC_EXIT_NOT_OK;
    }

    if (!MusicMetadata::updateStreamList())
        return GENERIC_EXIT_NOT_OK;

    return GENERIC_EXIT_OK;
}

static int CalcTrackLength(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("songid").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --songid option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    int songID = cmdline.toInt("songid");

    MusicMetadata *mdata = MusicMetadata::createFromID(songID);
    if (!mdata)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find metadata for trackid: %1").arg(songID));
        return GENERIC_EXIT_NOT_OK;
    }

    QString musicFile = mdata->getLocalFilename();

    if (musicFile.isEmpty() || !QFile::exists(musicFile))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find file for trackid: %1").arg(songID));
        return GENERIC_EXIT_NOT_OK;
    }

    AVFormatContext *inputFC = nullptr;
    AVInputFormat *fmt = nullptr;

    // Open track
    LOG(VB_GENERAL, LOG_DEBUG, QString("CalcTrackLength: Opening '%1'")
            .arg(musicFile));

    QByteArray inFileBA = musicFile.toLocal8Bit();

    int ret = avformat_open_input(&inputFC, inFileBA.constData(), fmt, nullptr);

    if (ret)
    {
        LOG(VB_GENERAL, LOG_ERR, "CalcTrackLength: Couldn't open input file" +
                                  ENO);
        return GENERIC_EXIT_NOT_OK;
    }

    // Getting stream information
    ret = avformat_find_stream_info(inputFC, nullptr);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("CalcTrackLength: Couldn't get stream info, error #%1").arg(ret));
        avformat_close_input(&inputFC);
        inputFC = nullptr;
        return GENERIC_EXIT_NOT_OK;;
    }

    std::chrono::seconds duration = 0s;
    long long time = 0;

    for (uint i = 0; i < inputFC->nb_streams; i++)
    {
        AVStream *st = inputFC->streams[i];
        std::array<char,256> buf {};

        const AVCodec *pCodec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!pCodec)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("avcodec_find_decoder fail for %1").arg(st->codecpar->codec_id));
            continue;
        }
        AVCodecContext *avctx = avcodec_alloc_context3(pCodec);
        avcodec_parameters_to_context(avctx, st->codecpar);
        avctx->pkt_timebase = st->time_base;

        avcodec_string(buf.data(), buf.size(), avctx, static_cast<int>(false));

        switch (inputFC->streams[i]->codecpar->codec_type)
        {
            case AVMEDIA_TYPE_AUDIO:
            {
                AVPacket *pkt = av_packet_alloc();
                if (pkt == nullptr)
                {
                    LOG(VB_GENERAL, LOG_ERR, "packet allocation failed");
                    break;
                }

                while (av_read_frame(inputFC, pkt) >= 0)
                {
                    if (pkt->stream_index == (int)i)
                        time = time + pkt->duration;

                    av_packet_unref(pkt);
                }

                av_packet_free(&pkt);

                duration = secondsFromFloat(time * av_q2d(inputFC->streams[i]->time_base));
                break;
            }

            default:
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Skipping unsupported codec %1 on stream %2")
                        .arg(inputFC->streams[i]->codecpar->codec_type).arg(i));
                break;
        }
        avcodec_free_context(&avctx);
    }

    // Close input file
    avformat_close_input(&inputFC);
    inputFC = nullptr;

    std::chrono::seconds dbLength = duration_cast<std::chrono::seconds>(mdata->Length());
    if (dbLength != duration)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("The length of this track in the database was %1s "
                                          "it is now %2s").arg(dbLength.count()).arg(duration.count()));

        // update the track length in the database
        mdata->setLength(duration);
        mdata->dumpToDatabase();

        // tell any clients that the metadata for this track has changed
        gCoreContext->SendMessage(QString("MUSIC_METADATA_CHANGED %1").arg(songID));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QString("The length of this track is unchanged %1s")
                                          .arg(dbLength.count()));
    }

    return GENERIC_EXIT_OK;
}

class LyricsGrabber
{
public:
    QString m_name;
    QString m_filename;
    int     m_priority {99};
};

static int FindLyrics(const MythUtilCommandLineParser &cmdline)
{
    // make sure our lyrics cache directory exists
    QString lyricsDir = GetConfDir() + "/MythMusic/Lyrics/";
    QDir dir(lyricsDir);
    if (!dir.exists())
        dir.mkpath(lyricsDir);

    if (cmdline.toString("songid").isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Missing --songid option");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    int songID = cmdline.toInt("songid");
    QString grabberName = "ALL";
    QString lyricsFile;
    QString artist;
    QString album;
    QString title;
    QString filename;

    if (!cmdline.toString("grabber").isEmpty())
        grabberName = cmdline.toString("grabber");

    if (ID_TO_REPO(songID) == RT_Database)
    {
        MusicMetadata *mdata = MusicMetadata::createFromID(songID);
        if (!mdata)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot find metadata for trackid: %1").arg(songID));
            return GENERIC_EXIT_NOT_OK;
        }

        QString musicFile = mdata->getLocalFilename();

        if (musicFile.isEmpty() || !QFile::exists(musicFile))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Cannot find file for trackid: %1").arg(songID));
            //return GENERIC_EXIT_NOT_OK;
        }

        // first check if we have already saved a lyrics file for this track
        lyricsFile = GetConfDir() + QString("/MythMusic/Lyrics/%1.txt").arg(songID);
        if (QFile::exists(lyricsFile))
        {
            // if the user specified a specific grabber assume they want to
            // re-search for the lyrics using the given grabber
            if (grabberName != "ALL")
                QFile::remove(lyricsFile);
            else
            {
                // load these lyrics to speed up future lookups
                QFile file(QLatin1String(qPrintable(lyricsFile)));
                QString lyrics;

                if (file.open(QIODevice::ReadOnly))
                {
                    QTextStream stream(&file);

                    while (!stream.atEnd())
                    {
                        lyrics.append(stream.readLine());
                    }

                    file.close();
                }

                // tell any clients that a lyrics file is available for this track
                gCoreContext->SendMessage(QString("MUSIC_LYRICS_FOUND %1 %2").arg(songID).arg(lyrics));

                return GENERIC_EXIT_OK;
            }
        }

        artist = mdata->Artist();
        album = mdata->Album();
        title = mdata->Title();
        filename = mdata->getLocalFilename();
    }
    else
    {
        // must be a CD or Radio Track
        if (cmdline.toString("artist").isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --artist option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        artist = cmdline.toString("artist");

        if (cmdline.toString("album").isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --album option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        album = cmdline.toString("album");

        if (cmdline.toString("title").isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "Missing --title option");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        title = cmdline.toString("title");
    }

    // not found so try the grabbers
    // first get a list of available grabbers
    QString  scriptDir = GetShareDir() + "metadata/Music/lyrics";
    QDir d(scriptDir);

    if (!d.exists())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find lyric scripts directory: %1").arg(scriptDir));
        gCoreContext->SendMessage(QString("MUSIC_LYRICS_ERROR NO_SCRIPTS_DIR"));
        return GENERIC_EXIT_NOT_OK;
    }

    d.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    d.setNameFilters(QStringList("*.py"));
    QFileInfoList list = d.entryInfoList();
    if (list.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find any lyric scripts in: %1").arg(scriptDir));
        gCoreContext->SendMessage(QString("MUSIC_LYRICS_ERROR NO_SCRIPTS_FOUND"));
        return GENERIC_EXIT_NOT_OK;
    }

    QStringList scripts;

    for (const auto& fi : std::as_const(list))
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Found lyric script at: %1").arg(fi.filePath()));
        scripts.append(fi.filePath());
    }

    QMap<int, LyricsGrabber> grabberMap;

    // query the grabbers to get their priority
    for (int x = 0; x < scripts.count(); x++)
    {
        QStringList args { scripts.at(x), "-v" };
        QProcess p;
        p.start(PYTHON_EXE, args);
        p.waitForFinished(-1);
        QString result = p.readAllStandardOutput();

        QDomDocument domDoc;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!domDoc.setContent(result, false, &errorMsg, &errorLine, &errorColumn))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("FindLyrics: Could not parse version from %1").arg(scripts.at(x)) +
                QString("\n\t\t\tError at line: %1  column: %2 msg: %3").arg(errorLine).arg(errorColumn).arg(errorMsg));
            continue;
        }
#else
        auto parseResult = domDoc.setContent(result);
        if (!parseResult)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("FindLyrics: Could not parse version from %1").arg(scripts.at(x)) +
                QString("\n\t\t\tError at line: %1  column: %2 msg: %3")
                .arg(parseResult.errorLine).arg(parseResult.errorColumn)
                .arg(parseResult.errorMessage));
            continue;
        }
#endif

        QDomNodeList itemList = domDoc.elementsByTagName("grabber");
        QDomNode itemNode = itemList.item(0);

        LyricsGrabber grabber;
        grabber.m_name = itemNode.namedItem(QString("name")).toElement().text();
        grabber.m_priority = itemNode.namedItem(QString("priority")).toElement().text().toInt();
        grabber.m_filename = scripts.at(x);

        grabberMap.insert(grabber.m_priority, grabber);
    }

    // try each grabber in turn until we find a match
    QMap<int, LyricsGrabber>::const_iterator i = grabberMap.constBegin();
    while (i != grabberMap.constEnd())
    {
        LyricsGrabber grabber = i.value();

        ++i;

        if (grabberName != "ALL" && grabberName != grabber.m_name)
            continue;

        LOG(VB_GENERAL, LOG_NOTICE, QString("Trying grabber: %1, Priority: %2").arg(grabber.m_name).arg(grabber.m_priority));
        QString statusMessage = QObject::tr("Searching '%1' for lyrics...").arg(grabber.m_name);
        gCoreContext->SendMessage(QString("MUSIC_LYRICS_STATUS %1 %2").arg(songID).arg(statusMessage));

        QProcess p;
        QStringList args { grabber.m_filename,
                           QString("--artist=%1").arg(artist),
                           QString("--album=%1").arg(album),
                           QString("--title=%1").arg(title),
                           QString("--filename=%1").arg(filename) };
        p.start(PYTHON_EXE, args);
        p.waitForFinished(-1);
        QString result = p.readAllStandardOutput();

        LOG(VB_GENERAL, LOG_DEBUG, QString("Grabber: %1, Exited with code: %2").arg(grabber.m_name).arg(p.exitCode()));

        if (p.exitCode() == 0)
        {
            LOG(VB_GENERAL, LOG_NOTICE, QString("Lyrics Found using: %1").arg(grabber.m_name));

            // save these lyrics to speed up future lookups if it is a DB track
            if (ID_TO_REPO(songID) == RT_Database)
            {
                QFile file(QLatin1String(qPrintable(lyricsFile)));

                if (file.open(QIODevice::WriteOnly))
                {
                    QTextStream stream(&file);
                    stream << result;
                    file.close();
                }
            }

            gCoreContext->SendMessage(QString("MUSIC_LYRICS_FOUND %1 %2").arg(songID).arg(result));
            return GENERIC_EXIT_OK;
        }
    }

    // if we got here we didn't find any lyrics
    gCoreContext->SendMessage(QString("MUSIC_LYRICS_NOTFOUND %1").arg(songID));

    return GENERIC_EXIT_OK;
}

void registerMusicUtils(UtilMap &utilMap)
{
    utilMap["updatemeta"] = &UpdateMeta;
    utilMap["extractimage"] = &ExtractImage;
    utilMap["scanmusic"] = &ScanMusic;
    utilMap["updateradiostreams"] = &UpdateRadioStreams;
    utilMap["calctracklen"] = &CalcTrackLength;
    utilMap["findlyrics"] = &FindLyrics;
}

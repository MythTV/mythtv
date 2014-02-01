// qt
#include <QDir>

// libmyth* headers
#include "exitcodes.h"
#include "mythlogging.h"
#include "storagegroup.h"
#include "musicmetadata.h"
#include "metaio.h"
#include "mythcontext.h"
#include "musicfilescanner.h"
#include "musicutils.h"

// mythutils headers
#include "commandlineparser.h"
#include "musicmetautils.h"

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
        ok = tagger->write(mdata);

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
    ImageType type = AlbumArtImages::getImageTypeFromName(cmdline.toString("imagetype"));

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


    if (!image->embedded || !tagger->supportsEmbeddedImages())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Either the image isn't embedded or the tagger doesn't support embedded images"));
        return GENERIC_EXIT_NOT_OK;
    }

    // find the tracks actual filename
    StorageGroup musicGroup("Music", gCoreContext->GetHostName(), false);
    QString trackFilename =  musicGroup.FindFile(mdata->Filename(false));

    // where are we going to save the image
    QString path;
    StorageGroup artGroup("MusicArt", gCoreContext->GetHostName());
    QStringList dirList = artGroup.GetDirList();
    if (dirList.size())
        path = artGroup.FindNextDirMostFree();

    if (!QDir(path).exists())
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot find a directory in the 'MusicArt' storage group to save to");
        return GENERIC_EXIT_NOT_OK;
    }

    path += "/AlbumArt/";
    QDir dir(path);

    QString filename = QString("%1-%2.jpg").arg(mdata->ID()).arg(AlbumArtImages::getTypeFilename(image->imageType));
    if (!QFile::exists(path + filename))
    {
        if (!dir.exists())
            dir.mkpath(path);

        QImage *saveImage = tagger->getAlbumArt(trackFilename, image->imageType);
        if (saveImage)
        {
            saveImage->save(path + filename, "JPEG");
            delete saveImage;
        }
    }

    delete tagger;

    // tell any clients that the metadata for this track has changed
    // TODO check we need this
    gCoreContext->SendMessage(QString("MUSIC_METADATA_CHANGED %1").arg(songID));

    return GENERIC_EXIT_OK;
}

static int ScanMusic(const MythUtilCommandLineParser &cmdline)
{
    MusicFileScanner *fscan = new MusicFileScanner();
    QStringList dirList;

    if (!StorageGroup::FindDirs("Music", gCoreContext->GetHostName(), &dirList))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find any directories in the 'Music' storage group");
        return GENERIC_EXIT_NOT_OK;
    }

    fscan->SearchDirs(dirList);
    delete fscan;

    return GENERIC_EXIT_OK;
}

void registerMusicUtils(UtilMap &utilMap)
{
    utilMap["updatemeta"] = &UpdateMeta;
    utilMap["extractimage"] = &ExtractImage;
    utilMap["scanmusic"] = &ScanMusic;
}

// libmyth* headers
#include "exitcodes.h"
#include "mythlogging.h"
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

static int ScanMusic(const MythUtilCommandLineParser &cmdline)
{
    MusicFileScanner *fscan = new MusicFileScanner();
    QString musicDir = getMusicDirectory();
    fscan->SearchDir(musicDir);
    delete fscan;

    return GENERIC_EXIT_OK;
}

void registerMusicUtils(UtilMap &utilMap)
{
    utilMap["updatemeta"] = &UpdateMeta;
    utilMap["scanmusic"] = &ScanMusic;
}

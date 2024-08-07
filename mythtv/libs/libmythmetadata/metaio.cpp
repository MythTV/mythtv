
// POSIX C headers
#include <utime.h>

// Qt
#include <QFileInfo>

// libmythmetadata
#include "metaio.h"
#include "musicmetadata.h"
#include "metaioid3.h"
#include "metaiooggvorbis.h"
#include "metaioflacvorbis.h"
#include "metaiomp4.h"
#include "metaiowavpack.h"
#include "metaioavfcomment.h"

// Libmyth
#include "libmyth/mythcontext.h"

const QString MetaIO::kValidFileExtensions(".mp3|.mp2|.ogg|.oga|.flac|.wma|.wav|.ac3|.oma|.omg|"
                                          ".atp|.ra|.dts|.aac|.m4a|.aa3|.tta|.mka|.aiff|.swa|.wv");

// static
MetaIO* MetaIO::createTagger(const QString& filename)
{
    QFileInfo fi(filename);
    QString extension = fi.suffix().toLower();

    if (extension.isEmpty() || !MetaIO::kValidFileExtensions.contains(extension))
    {
        LOG(VB_FILE, LOG_WARNING, QString("MetaIO: unknown extension: '%1'").arg(extension));
        return nullptr;
    }

    if (extension == "mp3" || extension == "mp2")
        return new MetaIOID3;
    if (extension == "ogg" || extension == "oga")
        return new MetaIOOggVorbis;
    if (extension == "flac")
    {
        auto *tagger = new MetaIOFLACVorbis;
        if (tagger->TagExists(filename))
            return tagger;
        delete tagger;
        return new MetaIOID3;
    }
    if (extension == "m4a")
        return new MetaIOMP4;
    if (extension == "wv")
        return new MetaIOWavPack;
    return new MetaIOAVFComment;
}

// static
MusicMetadata* MetaIO::readMetadata(const QString &filename)
{
    MusicMetadata *mdata = nullptr;
    MetaIO *tagger = MetaIO::createTagger(filename);
    bool ignoreID3 = (gCoreContext->GetNumSetting("Ignore_ID3", 0) == 1);

    if (tagger)
    {
        if (!ignoreID3)
            mdata = tagger->read(filename);

        if (ignoreID3 || !mdata)
            mdata = tagger->readFromFilename(filename);

        delete tagger;
    }

    if (!mdata)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIO::readMetadata(): Could not read '%1'")
                .arg(filename));
    }

    return mdata;
}

// static
MusicMetadata* MetaIO::getMetadata(const QString &filename)
{
    //FIXME: we need a relative path for createFromFilename()
    //       but readMetadata() needs an absolute path
    MusicMetadata *mdata = MusicMetadata::createFromFilename(filename);
    if (mdata)
        return mdata;

    return readMetadata(filename);
}

void MetaIO::readFromFilename(const QString &filename,
                              QString &artist, QString &album, QString &title,
                              QString &genre, int &tracknum)
{
    QString lfilename = filename;
    // Clear
    artist.clear();
    album.clear();
    title.clear();
    genre.clear();
    tracknum = 0;

    int part_num = 0;
    // Replace 
    lfilename.replace('_', ' ');
    lfilename = lfilename.section('.', 0, -2);
    QStringList fmt_list = m_filenameFormat.split("/");
    QStringList::iterator fmt_it = fmt_list.begin();

    // go through loop once to get minimum part number
    for (; fmt_it != fmt_list.end(); ++fmt_it, --part_num) {}

    // reset to go through loop for real
    fmt_it = fmt_list.begin();
    for(; fmt_it != fmt_list.end(); ++fmt_it, ++part_num)
    {
        QString part_str = lfilename.section( "/", part_num, part_num);

        if ( *fmt_it == "GENRE" )
            genre = part_str;
        else if ( *fmt_it == "ARTIST" )
            artist = part_str;
        else if ( *fmt_it == "ALBUM" )
            album = part_str;
        else if ( *fmt_it == "TITLE" )
            title = part_str;
        else if ( *fmt_it == "TRACK_TITLE" )
        {
            QStringList tracktitle_list = part_str.split("-");
            if (tracktitle_list.size() > 1)
            {
                tracknum = tracktitle_list[0].toInt();
                title = tracktitle_list[1].simplified();
            }
            else
            {
                title = part_str;
            }
        }
        else if ( *fmt_it == "ARTIST_TITLE" )
        {
            QStringList artisttitle_list = part_str.split("-");
            if (artisttitle_list.size() > 1)
            {
                artist = artisttitle_list[0].simplified();
                title = artisttitle_list[1].simplified();
            }
            else
            {
                if (title.isEmpty())
                    title = part_str;
                if (artist.isEmpty())
                    artist = part_str;
            }
        }
    }
}

MusicMetadata* MetaIO::readFromFilename(const QString &filename, bool blnLength)
{
    QString artist;
    QString album;
    QString title;
    QString genre;
    int tracknum = 0;

    readFromFilename(filename, artist, album, title, genre, tracknum);

    std::chrono::milliseconds length = (blnLength) ? getTrackLength(filename) : 0ms;

    auto *retdata = new MusicMetadata(filename, artist, "", album, title, genre,
                                      0, tracknum, length);

    return retdata;
}

/*!
* \brief Reads MusicMetadata based on the folder/filename.
*
* \param metadata MusicMetadata Pointer
*/
void MetaIO::readFromFilename(MusicMetadata* metadata)
{
    QString artist;
    QString album;
    QString title;
    QString genre;
    int tracknum = 0;

    const QString filename = metadata->Filename(false);

    if (filename.isEmpty())
        return;

    readFromFilename(filename, artist, album, title, genre, tracknum);

    if (metadata->Artist().isEmpty())
        metadata->setArtist(artist);

    if (metadata->Album().isEmpty())
        metadata->setAlbum(album);

    if (metadata->Title().isEmpty())
        metadata->setTitle(title);

    if (metadata->Genre().isEmpty())
        metadata->setGenre(genre);

    if (metadata->Track() <= 0)
        metadata->setTrack(tracknum);
}

void MetaIO::saveTimeStamps(void)
{
    if (stat(m_filename.toLocal8Bit().constData(), &m_fileinfo) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIO::saveTimeStamps: failed to stat file: %1").arg(m_filename) + ENO);
    }
}

void MetaIO::restoreTimeStamps(void)
{
    struct utimbuf new_times {m_fileinfo.st_atime, m_fileinfo.st_mtime};

    if (utime(m_filename.toLocal8Bit().constData(), &new_times) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("MetaIO::restoreTimeStamps: failed to utime file: %1").arg(m_filename) + ENO);
    }
}


// Mythmusic
#include "metaio.h"
#include "metadata.h"

// Libmyth
#include <mythcontext.h>

/*!
 * \brief Constructor
 */
MetaIO::MetaIO()
{
    mFilenameFormat = gCoreContext->GetSetting("NonID3FileNameFormat").toUpper();
}

/*!
 * \brief Destructor
 */
MetaIO::~MetaIO()
{
}

/*!
 * \brief Reads Metadata based on the folder/filename.
 *
 * \param filename The filename to try and determine metadata for.
 * \returns Metadata Pointer, or NULL on error.
 */
void MetaIO::readFromFilename(QString filename,
                              QString &artist, QString &album, QString &title,
                              QString &genre, int &tracknum)
{
    // Clear
    artist.clear();
    album.clear();
    title.clear();
    genre.clear();
    tracknum = 0;
    
    int part_num = 0;
    // Replace 
    filename.replace('_', ' ');
    filename.section('.', 0, -2);
    QStringList fmt_list = mFilenameFormat.split("/");
    QStringList::iterator fmt_it = fmt_list.begin();

    // go through loop once to get minimum part number
    for (; fmt_it != fmt_list.end(); fmt_it++, part_num--) {}

    // reset to go through loop for real
    fmt_it = fmt_list.begin();
    for(; fmt_it != fmt_list.end(); fmt_it++, part_num++)
    {
        QString part_str = filename.section( "/", part_num, part_num);

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
                title = part_str;
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
                    title = part_str;
            }
        }
    }
}

/*!
 * \brief Reads Metadata based on the folder/filename.
 *
 * \note Just an overloaded wrapper around the other method above.
 *
 * \param filename The filename to try and determine metadata for.
 * \returns Metadata Pointer, or NULL on error.
 */
Metadata* MetaIO::readFromFilename(QString filename, bool blnLength)
{
    QString artist, album, title, genre;
    int tracknum = 0, length = 0;

    readFromFilename(filename, artist, album, title, genre, tracknum);

    if (blnLength)
        length = getTrackLength(filename);

    Metadata *retdata = new Metadata(filename, artist, "", album, title, genre,
                                     0, tracknum, length);

    return retdata;
}

/*!
* \brief Reads Metadata based on the folder/filename.
*
* \param metadata Metadata Pointer
*/
void MetaIO::readFromFilename(Metadata* metadata)
{
    QString artist, album, title, genre;
    int tracknum = 0;

    const QString filename = metadata->Filename();

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

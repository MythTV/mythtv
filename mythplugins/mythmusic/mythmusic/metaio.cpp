#include <iostream>

#include "metaio.h"
#include "metadata.h"
#include <mythtv/mythcontext.h>

//==========================================================================
/*!
 * \brief Constructor
 *
 * \param fileExtension The extension of the files the derived class uses.
 */
MetaIO::MetaIO(QString fileExtension)
    : mFileExtension(fileExtension)
{
    mFilenameFormat = gContext->GetSetting("NonID3FileNameFormat").upper();
}


//==========================================================================
/*!
 * \brief Destructor
 */
MetaIO::~MetaIO()
{ 
}


//==========================================================================
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
    artist = album = title = genre = "";
    tracknum = 0;

    static QString regext = mFileExtension + "$";
    int part_num = 0;
    filename.replace(QRegExp(QString("_")), QString(" "));
    filename.replace(QRegExp(regext, FALSE), QString(""));
    QStringList fmt_list = QStringList::split("/", mFilenameFormat);
    QStringList::iterator fmt_it = fmt_list.begin();

    // go through loop once to get minimum part number
    for(; fmt_it != fmt_list.end(); fmt_it++, part_num--);

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
            QStringList tracktitle_list = QStringList::split("-", part_str);
            if (tracktitle_list.size() > 1)
            {
                tracknum = tracktitle_list[0].toInt();
                title = tracktitle_list[1].simplifyWhiteSpace();
            }
            else
                title = part_str;
        }
        else if ( *fmt_it == "ARTIST_TITLE" ) 
        {
            QStringList artisttitle_list = QStringList::split("-", part_str);
            if (artisttitle_list.size() > 1)
            {
                artist = artisttitle_list[0].simplifyWhiteSpace();
                title = artisttitle_list[1].simplifyWhiteSpace();
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


//==========================================================================
/*!
 * \brief Reads Metadata based on the folder/filename.
 *
 * \note Just an overloaded wrapper around the other method above.
 *
 * \param filename The filename to try and determin metadata for.
 * \returns Metadata Pointer, or NULL on error.
 */
Metadata* MetaIO::readFromFilename(QString filename, bool blnLength)
{
    QString artist = "", album = "", title = "", genre = "";
    int tracknum = 0, length = 0;

    readFromFilename(filename, artist, album, title, genre, tracknum);

    if (blnLength)
        length = getTrackLength(filename);

    Metadata *retdata = new Metadata(filename, artist, "", album, title, genre,
                                     0, tracknum, length);

    return retdata;
}

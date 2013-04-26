
// Libmyth
#include <mythcontext.h>

// Taglib
#include <xiphcomment.h>

// libmythmetadata
#include "metaioflacvorbis.h"
#include "musicmetadata.h"
#include "musicutils.h"

MetaIOFLACVorbis::MetaIOFLACVorbis(void)
    : MetaIOTagLib()
{
}

MetaIOFLACVorbis::~MetaIOFLACVorbis(void)
{
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::FLAC::File *MetaIOFLACVorbis::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    TagLib::FLAC::File *flacfile =
                            new TagLib::FLAC::File(fname.constData());

    if (!flacfile->isOpen())
    {
        delete flacfile;
        flacfile = NULL;
    }

    return flacfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOFLACVorbis::write(const MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    TagLib::FLAC::File *flacfile = OpenFile(mdata->Filename());

    if (!flacfile)
        return false;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment(true);

    if (!tag)
    {
        delete flacfile;
        return false;
    }

    WriteGenericMetadata(tag, mdata);

    // Compilation
    if (mdata->Compilation())
    {
        tag->addField("MUSICBRAINZ_ALBUMARTISTID",
                          MYTH_MUSICBRAINZ_ALBUMARTIST_UUID, true);
        tag->addField("COMPILATION_ARTIST",
                        QStringToTString(mdata->CompilationArtist()), true);
    }
    else
    {
        // Don't remove the musicbrainz field unless it indicated a compilation
        if (tag->contains("MUSICBRAINZ_ALBUMARTISTID") &&
            (tag->fieldListMap()["MUSICBRAINZ_ALBUMARTISTID"].toString() ==
                MYTH_MUSICBRAINZ_ALBUMARTIST_UUID))
        {
            tag->removeField("MUSICBRAINZ_ALBUMARTISTID");
        }
        tag->removeField("COMPILATION_ARTIST");
    }

    bool result = flacfile->save();

    if (flacfile)
        delete flacfile;

    return (result);
}

/*!
 * \copydoc MetaIO::read()
 */
MusicMetadata* MetaIOFLACVorbis::read(const QString &filename)
{
    TagLib::FLAC::File *flacfile = OpenFile(filename);

    if (!flacfile)
        return NULL;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment();

    if (!tag)
    {
        delete flacfile;
        return NULL;
    }

    MusicMetadata *metadata = new MusicMetadata(filename);

    ReadGenericMetadata(tag, metadata);

    bool compilation = false;

    if (tag->contains("COMPILATION_ARTIST"))
    {
        QString compilation_artist = TStringToQString(
            tag->fieldListMap()["COMPILATION_ARTIST"].toString()).trimmed();
        if (compilation_artist != metadata->Artist())
        {
            metadata->setCompilationArtist(compilation_artist);
            compilation = true;
        }
    }

    if (!compilation && tag->contains("MUSICBRAINZ_ALBUMARTISTID"))
    {
        QString musicbrainzcode = TStringToQString(
        tag->fieldListMap()["MUSICBRAINZ_ALBUMARTISTID"].toString()).trimmed();
        if (musicbrainzcode == MYTH_MUSICBRAINZ_ALBUMARTIST_UUID)
            compilation = true;
    }

    metadata->setCompilation(compilation);

    if (metadata->Length() <= 0)
        metadata->setLength(getTrackLength(flacfile));

    delete flacfile;

    return metadata;
}

bool MetaIOFLACVorbis::TagExists(const QString &filename)
{
    TagLib::FLAC::File *flacfile = OpenFile(filename);

    if (!flacfile)
        return false;

    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment(false);

    bool retval = false;
    if (tag && !tag->isEmpty())
        retval = true;

    delete flacfile;

    return retval;
}

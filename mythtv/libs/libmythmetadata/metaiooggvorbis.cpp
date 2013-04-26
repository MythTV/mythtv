
// libmythmetadata
#include "metaiooggvorbis.h"
#include "musicmetadata.h"
#include "musicutils.h"

// Libmyth
#include <mythcontext.h>

MetaIOOggVorbis::MetaIOOggVorbis(void)
    : MetaIOTagLib()
{
}

MetaIOOggVorbis::~MetaIOOggVorbis(void)
{
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::Ogg::Vorbis::File *MetaIOOggVorbis::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    TagLib::Ogg::Vorbis::File *oggfile =
                            new TagLib::Ogg::Vorbis::File(fname.constData());

    if (!oggfile->isOpen())
    {
        delete oggfile;
        oggfile = NULL;
    }

    return oggfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOOggVorbis::write(const MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    TagLib::Ogg::Vorbis::File *oggfile = OpenFile(mdata->Filename());

    if (!oggfile)
        return false;

    TagLib::Ogg::XiphComment *tag = oggfile->tag();

    if (!tag)
    {
        delete oggfile;
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

    bool result = oggfile->save();

    if (oggfile)
        delete oggfile;

    return (result);
}

/*!
* \copydoc MetaIO::read()
*/
MusicMetadata* MetaIOOggVorbis::read(const QString &filename)
{
    TagLib::Ogg::Vorbis::File *oggfile = OpenFile(filename);

    if (!oggfile)
        return NULL;

    TagLib::Ogg::XiphComment *tag = oggfile->tag();

    if (!tag)
    {
        delete oggfile;
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
        metadata->setLength(getTrackLength(oggfile));

    delete oggfile;

    return metadata;
}

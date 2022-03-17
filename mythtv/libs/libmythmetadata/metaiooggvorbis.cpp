
// libmythmetadata
#include "metaiooggvorbis.h"
#include "musicmetadata.h"
#include "musicutils.h"

// Libmyth
#include "libmyth/mythcontext.h"

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::Ogg::Vorbis::File *MetaIOOggVorbis::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    auto *oggfile = new TagLib::Ogg::Vorbis::File(fname.constData());

    if (!oggfile->isOpen())
    {
        delete oggfile;
        oggfile = nullptr;
    }

    return oggfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOOggVorbis::write(const QString &filename, MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    m_filename = filename;

    if (m_filename.isEmpty())
        return false;

    TagLib::Ogg::Vorbis::File *oggfile = OpenFile(m_filename);

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
            tag->removeFields("MUSICBRAINZ_ALBUMARTISTID");
        }
        tag->removeFields("COMPILATION_ARTIST");
    }

    saveTimeStamps();
    bool result = oggfile->save();
    restoreTimeStamps();

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
        return nullptr;

    TagLib::Ogg::XiphComment *tag = oggfile->tag();

    if (!tag)
    {
        delete oggfile;
        return nullptr;
    }

    auto *metadata = new MusicMetadata(filename);

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

    if (metadata->Length() <= 0ms)
        metadata->setLength(getTrackLength(oggfile));

    delete oggfile;

    return metadata;
}

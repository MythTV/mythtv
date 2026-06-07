
// libmythmetadata
#include "metaiooggopus.h"
#include "musicmetadata.h"
#include "musicutils.h"

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
TagLib::Ogg::Opus::File *MetaIOOggOpus::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    auto *opusfile = new TagLib::Ogg::Opus::File(fname.constData());

    if (!opusfile->isOpen())
    {
        delete opusfile;
        opusfile = nullptr;
    }

    return opusfile;
}


/*!
 * \copydoc MetaIO::write()
 */
bool MetaIOOggOpus::write(const QString &filename, MusicMetadata* mdata)
{
    if (!mdata)
        return false;

    if (filename.isEmpty())
        return false;

    m_filename = filename;

    TagLib::Ogg::Opus::File *opusfile = OpenFile(m_filename);

    if (!opusfile)
        return false;

    TagLib::Ogg::XiphComment *tag = opusfile->tag();

    if (!tag)
    {
        delete opusfile;
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
    bool result = opusfile->save();
    restoreTimeStamps();

    delete opusfile;

    return (result);
}

/*!
* \copydoc MetaIO::read()
*/
MusicMetadata* MetaIOOggOpus::read(const QString &filename)
{
    TagLib::Ogg::Opus::File *opusfile = OpenFile(filename);

    if (!opusfile)
        return nullptr;

    TagLib::Ogg::XiphComment *tag = opusfile->tag();

    if (!tag)
    {
        delete opusfile;
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
        metadata->setLength(getTrackLength(opusfile));

    delete opusfile;

    return metadata;
}

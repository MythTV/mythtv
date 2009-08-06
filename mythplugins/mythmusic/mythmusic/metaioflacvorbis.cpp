
#include "metaioflacvorbis.h"
#include "metadata.h"

#include <mythtv/mythcontext.h>

#include <xiphcomment.h>

MetaIOFLACVorbis::MetaIOFLACVorbis(void)
    : MetaIOTagLib(".flac")
{
}

MetaIOFLACVorbis::~MetaIOFLACVorbis(void)
{
}

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
 * \brief Writes metadata back to a file
 *
 * \param mdata A pointer to a Metadata object
 * \param exclusive Flag to indicate if only the data in mdata should be
 *                  in the file. If false, any unrecognised tags already
 *                  in the file will be maintained.
 * \returns Boolean to indicate success/failure.
 */
bool MetaIOFLACVorbis::write(Metadata* mdata, bool exclusive)
{
    (void) exclusive;

    if (!mdata)
        return false;

    TagLib::FLAC::File *flacfile = OpenFile(mdata->Filename());
    
    if (!flacfile)
        return false;
    
    TagLib::Ogg::XiphComment *tag = flacfile->xiphComment();
    
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
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOFLACVorbis::read(QString filename)
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
    
    Metadata *metadata = new Metadata(filename);
    
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
    {
        TagLib::FileRef *fileref = new TagLib::FileRef(flacfile);
        metadata->setLength(getTrackLength(fileref));
        // FileRef takes ownership of flacfile, and is responsible for it's
        // deletion. Messy.
        delete fileref;
    }
    else
        delete flacfile;
    
    return metadata;
}

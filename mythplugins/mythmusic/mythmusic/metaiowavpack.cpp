#include <math.h>

#include "metaiowavpack.h"
#include "metadata.h"

#include <apetag.h>
#include <apeitem.h>

#include <mythtv/mythcontext.h>

MetaIOWavPack::MetaIOWavPack(void)
    : MetaIOTagLib(".wv")
{
}

MetaIOWavPack::~MetaIOWavPack(void)
{
}

TagLib::WavPack::File *MetaIOWavPack::OpenFile(const QString &filename)
{
    QByteArray fname = filename.toLocal8Bit();
    TagLib::WavPack::File *wpfile = new TagLib::WavPack::File(fname.constData());
    
    if (!wpfile->isOpen())
    {
        delete wpfile;
        wpfile = NULL;
    }
    
    return wpfile;
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
bool MetaIOWavPack::write(Metadata* mdata, bool exclusive)
{
    (void) exclusive;

    if (!mdata)
        return false;

    TagLib::WavPack::File *wpfile = OpenFile(mdata->Filename());
    
    if (!wpfile)
        return false;
    
    TagLib::APE::Tag *tag = wpfile->APETag();
    
    if (!tag)
    {
        delete wpfile;
        return false;
    }
    
    WriteGenericMetadata(tag, mdata);

    // Compilation Artist ("Album artist")
    if (mdata->Compilation())
    {
        TagLib::String key = "Album artist";
        TagLib::APE::Item item = TagLib::APE::Item(key,
            QStringToTString(mdata->CompilationArtist()));
        tag->setItem(key, item);
    }
    else
        tag->removeItem("Album artist");

    bool result = wpfile->save();

    if (wpfile)
        delete wpfile;

    return (result);
}

/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOWavPack::read(QString filename)
{
    TagLib::WavPack::File *wpfile = OpenFile(filename);
    
    if (!wpfile)
        return NULL;
    
    TagLib::APE::Tag *tag = wpfile->APETag();
    
    if (!tag)
    {
        delete wpfile;
        return NULL;
    }
    
    Metadata *metadata = new Metadata(filename);
    
    ReadGenericMetadata(tag, metadata);
    
    bool compilation = false;

    // Compilation Artist ("Album artist")
    if(tag->itemListMap().contains("Album artist"))
    {
        compilation = true;
        QString compilation_artist = TStringToQString(
                    tag->itemListMap()["Album artist"].toString()).trimmed();
        metadata->setCompilationArtist(compilation_artist);
    }

    metadata->setCompilation(compilation);

    if (metadata->Length() <= 0)
    {
        TagLib::FileRef *fileref = new TagLib::FileRef(wpfile);
        metadata->setLength(getTrackLength(fileref));
        // FileRef takes ownership of wpfile, and is responsible for it's
        // deletion. Messy.
        delete fileref;
    }
    else
        delete wpfile;
    
    return metadata;
}

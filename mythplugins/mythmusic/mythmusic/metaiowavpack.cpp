#include <math.h>

#include "metaiowavpack.h"
#include "metadata.h"

#include <apetag.h>
#include <apeitem.h>

#include <mythcontext.h>

MetaIOWavPack::MetaIOWavPack(void)
    : MetaIOTagLib()
{
}

MetaIOWavPack::~MetaIOWavPack(void)
{
}

/*!
* \brief Open the file to read the tag
*
* \param filename The filename
* \returns A taglib file object for this format
*/
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
* \copydoc MetaIO::write()
*/
bool MetaIOWavPack::write(const Metadata* mdata)
{
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
* \copydoc MetaIO::read()
*/
Metadata* MetaIOWavPack::read(const QString &filename)
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
        metadata->setLength(getTrackLength(wpfile));
    else
        delete wpfile;
    
    return metadata;
}

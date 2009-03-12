#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <attachedpictureframe.h>
#include <mpegfile.h>
#include <mpegproperties.h>

#include <QList>

#include "metaio.h"
#include "metadata.h"

using TagLib::MPEG::File;
using TagLib::Tag;
using TagLib::ID3v2::UserTextIdentificationFrame;
using TagLib::ID3v2::TextIdentificationFrame;
using TagLib::ID3v2::AttachedPictureFrame;
using TagLib::String;
using TagLib::MPEG::Properties;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOTagLib : public MetaIO
{
public:
    MetaIOTagLib(void);
    virtual ~MetaIOTagLib(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);

    static QImage getAlbumArt(QString filename, ImageType type);

private:

    int getTrackLength(QString filename);

    AlbumArtList readAlbumArt(TagLib::ID3v2::Tag *tag);
    UserTextIdentificationFrame* find(TagLib::ID3v2::Tag *tag, const String &description);
};

#endif

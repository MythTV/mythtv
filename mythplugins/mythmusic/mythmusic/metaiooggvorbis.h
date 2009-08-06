#ifndef METAIOOGGVORBIS_H_
#define METAIOOGGVORBIS_H_

#include "metaiotaglib.h"
#include "metadata.h"

#include <vorbisfile.h>

#include <QList>

using TagLib::Tag;
using TagLib::String;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOOggVorbis : public MetaIOTagLib
{
public:
    MetaIOOggVorbis(void);
    virtual ~MetaIOOggVorbis(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);

private:
    TagLib::Ogg::Vorbis::File *OpenFile(const QString &filename);
};

#endif

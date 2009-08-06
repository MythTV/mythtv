#ifndef METAIOFLACVORBIS_H_
#define METAIOFLACVORBIS_H_

#include "metaiotaglib.h"
#include "metadata.h"

#include <flacfile.h>

#include <QList>

using TagLib::Tag;
using TagLib::String;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOFLACVorbis : public MetaIOTagLib
{
public:
    MetaIOFLACVorbis(void);
    virtual ~MetaIOFLACVorbis(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);

private:
    TagLib::FLAC::File *OpenFile(const QString &filename);
};

#endif

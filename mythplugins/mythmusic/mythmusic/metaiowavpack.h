#ifndef METAIOWAVPACK_H_
#define METAIOWAVPACK_H_

#include "metaio.h"
#include "metadata.h"

#include <wavpackfile.h>

#include <QList>

//using TagLib::WavPack::File;
using TagLib::Tag;
using TagLib::String;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOWavPack : public MetaIO
{
public:
    MetaIOWavPack(void);
    virtual ~MetaIOWavPack(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);

private:

    int getTrackLength(QString filename);
};

#endif

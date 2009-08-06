#ifndef METAIOWAVPACK_H_
#define METAIOWAVPACK_H_

#include "metaiotaglib.h"
#include "metadata.h"

#include <wavpackfile.h>

#include <QList>

using TagLib::Tag;
using TagLib::String;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOWavPack : public MetaIOTagLib
{
public:
    MetaIOWavPack(void);
    virtual ~MetaIOWavPack(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);

private:
    TagLib::WavPack::File *OpenFile(const QString &filename);
};

#endif

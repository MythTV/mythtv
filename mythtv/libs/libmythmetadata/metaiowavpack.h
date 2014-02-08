#ifndef METAIOWAVPACK_H_
#define METAIOWAVPACK_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <wavpackfile.h>

using TagLib::Tag;
using TagLib::String;

/*!
* \class MetaIOWavPack
*
* \brief Read and write metadata in Wavpack APE tags
*
* N.B. No write support
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOWavPack : public MetaIOTagLib
{
public:
    MetaIOWavPack(void);
    virtual ~MetaIOWavPack(void);

    bool write(const QString &filename, MusicMetadata* mdata);
    MusicMetadata* read(const QString &filename);

private:
    TagLib::WavPack::File *OpenFile(const QString &filename);
};

#endif

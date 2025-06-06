#ifndef METAIOWAVPACK_H_
#define METAIOWAVPACK_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/wavpackfile.h>

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
    MetaIOWavPack(void) = default;
    ~MetaIOWavPack(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIOTagLib
    MusicMetadata* read(const QString &filename) override; // MetaIOTagLib

private:
    static TagLib::WavPack::File *OpenFile(const QString &filename);
};

#endif

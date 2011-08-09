#ifndef METAIOWAVPACK_H_
#define METAIOWAVPACK_H_

// Mythmusic
#include "metaiotaglib.h"
#include "metadata.h"

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
class MetaIOWavPack : public MetaIOTagLib
{
public:
    MetaIOWavPack(void);
    virtual ~MetaIOWavPack(void);

    bool write(const Metadata* mdata);
    Metadata* read(const QString &filename);

private:
    TagLib::WavPack::File *OpenFile(const QString &filename);
};

#endif

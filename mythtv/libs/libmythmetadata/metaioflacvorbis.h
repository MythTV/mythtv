#ifndef METAIOFLACVORBIS_H_
#define METAIOFLACVORBIS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <flacfile.h>

using TagLib::Tag;
using TagLib::String;

/*!
* \class MetaIOFLACVorbis
*
* \brief Read and write Vorbis (Xiph) tags in a FLAC file
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOFLACVorbis : public MetaIOTagLib
{
public:
    MetaIOFLACVorbis(void);
    virtual ~MetaIOFLACVorbis(void);

    bool write(const MusicMetadata* mdata);
    MusicMetadata* read(const QString &filename);

    virtual bool TagExists(const QString &filename);

private:
    TagLib::FLAC::File *OpenFile(const QString &filename);
};

#endif

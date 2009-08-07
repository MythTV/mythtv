#ifndef METAIOFLACVORBIS_H_
#define METAIOFLACVORBIS_H_

// Mythmusic
#include "metaiotaglib.h"
#include "metadata.h"

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
class MetaIOFLACVorbis : public MetaIOTagLib
{
public:
    MetaIOFLACVorbis(void);
    virtual ~MetaIOFLACVorbis(void);

    bool write(Metadata* mdata);
    Metadata* read(QString filename);

private:
    TagLib::FLAC::File *OpenFile(const QString &filename);
};

#endif

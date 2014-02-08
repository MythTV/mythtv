#ifndef METAIOOGGVORBIS_H_
#define METAIOOGGVORBIS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <vorbisfile.h>

using TagLib::Tag;
using TagLib::String;

/*!
* \class MetaIOOggVorbis
*
* \brief Read and write Vorbis (Xiph) tags in an Ogg container
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOOggVorbis : public MetaIOTagLib
{
  public:
    MetaIOOggVorbis(void);
    ~MetaIOOggVorbis(void);

    bool write(const QString &filename, MusicMetadata* mdata);
    MusicMetadata* read(const QString &filename);

  private:
    TagLib::Ogg::Vorbis::File *OpenFile(const QString &filename);
};

#endif

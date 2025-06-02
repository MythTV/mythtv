#ifndef METAIOOGGVORBIS_H_
#define METAIOOGGVORBIS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/vorbisfile.h>

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
    MetaIOOggVorbis(void) = default;
    ~MetaIOOggVorbis(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIOTagLib
    MusicMetadata* read(const QString &filename) override; // MetaIOTagLib

  private:
    static TagLib::Ogg::Vorbis::File *OpenFile(const QString &filename);
};

#endif

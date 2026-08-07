#ifndef METAIOOGGOPUS_H_
#define METAIOOGGOPUS_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/opusfile.h>

/*!
* \class MetaIOOggOpus
*
* \brief Read and write Opus (Xiph) tags in an Ogg container
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOOggOpus : public MetaIOTagLib
{
  public:
    MetaIOOggOpus(void) = default;
    ~MetaIOOggOpus(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIOTagLib
    MusicMetadata* read(const QString &filename) override; // MetaIOTagLib

  private:
    static TagLib::Ogg::Opus::File *OpenFile(const QString &filename);
};

#endif

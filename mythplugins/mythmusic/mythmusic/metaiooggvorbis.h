#ifndef METAIOOGGVORBIS_H_
#define METAIOOGGVORBIS_H_

// Mythmusic
#include "metaiotaglib.h"
#include "metadata.h"

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
class MetaIOOggVorbis : public MetaIOTagLib
{
  public:
    MetaIOOggVorbis(void);
    ~MetaIOOggVorbis(void);

    bool write(Metadata* mdata);
    Metadata* read(QString filename);

  private:
    TagLib::Ogg::Vorbis::File *OpenFile(const QString &filename);
};

#endif

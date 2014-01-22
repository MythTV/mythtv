#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

// libmythmetadata
#include "metaio.h"
#include "musicmetadata.h"

// Taglib
#include <tfile.h>

using TagLib::File;
using TagLib::Tag;
using TagLib::String;

/*!
 * \class MetaIOTagLib
 *
 * \brief Base for Taglib metadata classes
 */
class META_PUBLIC MetaIOTagLib : public MetaIO
{
  public:
    MetaIOTagLib(void);
    virtual ~MetaIOTagLib(void);

    virtual bool write(MusicMetadata* mdata) = 0;
    virtual MusicMetadata* read(const QString &filename) = 0;

  protected:
    int getTrackLength(TagLib::File *file);
    int getTrackLength(const QString &filename);
    void ReadGenericMetadata(TagLib::Tag *tag, MusicMetadata *metadata);
    void WriteGenericMetadata(TagLib::Tag *tag, const MusicMetadata *metadata);
};

#endif

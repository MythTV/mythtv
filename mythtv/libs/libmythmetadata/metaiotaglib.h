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
    MetaIOTagLib(void) : MetaIO() {}
    virtual ~MetaIOTagLib(void) = default;

    bool write(const QString &filename, MusicMetadata* mdata) override = 0; // MetaIO
    MusicMetadata* read(const QString &filename) override = 0; // MetaIO

  protected:
    static int getTrackLength(TagLib::File *file);
    int getTrackLength(const QString &filename) override; // MetaIO
    void ReadGenericMetadata(TagLib::Tag *tag, MusicMetadata *metadata);
    static void WriteGenericMetadata(TagLib::Tag *tag, const MusicMetadata *metadata);
};

#endif

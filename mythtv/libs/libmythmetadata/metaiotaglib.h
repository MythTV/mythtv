#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

// libmythmetadata
#include "metaio.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/tfile.h>

/*!
 * \class MetaIOTagLib
 *
 * \brief Base for Taglib metadata classes
 */
class META_PUBLIC MetaIOTagLib : public MetaIO
{
  public:
    MetaIOTagLib(void) = default;
    ~MetaIOTagLib(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override = 0; // MetaIO
    MusicMetadata* read(const QString &filename) override = 0; // MetaIO

  protected:
    static std::chrono::milliseconds getTrackLength(TagLib::File *file);
    std::chrono::milliseconds getTrackLength(const QString &filename) override; // MetaIO
    void ReadGenericMetadata(TagLib::Tag *tag, MusicMetadata *metadata);
    static void WriteGenericMetadata(TagLib::Tag *tag, const MusicMetadata *metadata);
};

#endif

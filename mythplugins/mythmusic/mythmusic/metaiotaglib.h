#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

// MythMusic
#include "metaio.h"
#include "metadata.h"

// Taglib
#include <tfile.h>
#include <fileref.h>

using TagLib::File;
using TagLib::Tag;
using TagLib::String;

/*!
 * \class MetaIOTagLib
 *
 * \brief Base for Taglib metadata classes
 */
class MetaIOTagLib : public MetaIO
{
  public:
    MetaIOTagLib(void);
    virtual ~MetaIOTagLib(void);

    virtual bool write(Metadata* mdata) = 0;
    virtual Metadata* read(QString filename) = 0;

  protected:
    int getTrackLength(TagLib::FileRef *file);
    int getTrackLength(QString filename);
    void ReadGenericMetadata(TagLib::Tag *tag, Metadata *metadata);
    void WriteGenericMetadata(TagLib::Tag *tag, Metadata *metadata);
};

#endif

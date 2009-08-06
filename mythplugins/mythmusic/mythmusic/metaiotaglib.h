#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

#include "metaio.h"
#include "metadata.h"

// Taglib
#include <tfile.h>
#include <fileref.h>

#include <QList>

using TagLib::File;
using TagLib::Tag;
using TagLib::String;

typedef QList<struct AlbumArtImage> AlbumArtList;

class MetaIOTagLib : public MetaIO
{
  public:
    MetaIOTagLib(QString fileExtension);
    virtual ~MetaIOTagLib(void);

    virtual bool write(Metadata* mdata, bool exclusive = false);
    virtual Metadata* read(QString filename);
    
  protected:
    int getTrackLength(TagLib::FileRef *file);
    int getTrackLength(QString filename);
    void ReadGenericMetadata(TagLib::Tag *tag, Metadata *metadata);
    void WriteGenericMetadata(TagLib::Tag *tag, Metadata *metadata);
};

#endif

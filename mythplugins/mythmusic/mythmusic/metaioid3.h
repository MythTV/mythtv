#ifndef METAIOID3_H_
#define METAIOID3_H_

// MythMusic
#include "metaiotaglib.h"
#include "metadata.h"

// Taglib
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <attachedpictureframe.h>
#include <mpegproperties.h>
#include <mpegfile.h>
#include <tfile.h>

// QT
#include <QList>

using TagLib::ID3v2::UserTextIdentificationFrame;
using TagLib::ID3v2::TextIdentificationFrame;
using TagLib::ID3v2::AttachedPictureFrame;
using TagLib::MPEG::Properties;

typedef QList<struct AlbumArtImage> AlbumArtList;

/*!
* \class MetaIOID3
*
* \brief Read and write metadata in MPEG (mp3) ID3V2 tags
*
* Will read ID3v1 but always writes ID3v2.4 compliant tags.
*
* \copydetails MetaIO
*/
class MetaIOID3 : public MetaIOTagLib
{
  public:
    MetaIOID3(void);
    virtual ~MetaIOID3(void);
    
    bool write(Metadata* mdata);
    Metadata* read(QString filename);
    static QImage getAlbumArt(QString filename, ImageType type);

  private:
    TagLib::MPEG::File *OpenFile(const QString &filename);

    AlbumArtList readAlbumArt(TagLib::ID3v2::Tag *tag);
    UserTextIdentificationFrame* find(TagLib::ID3v2::Tag *tag, const String &description);
};

#endif

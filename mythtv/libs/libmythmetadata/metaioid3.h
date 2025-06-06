#ifndef METAIOID3_H_
#define METAIOID3_H_

// libmythmetadata
#include "metaiotaglib.h"
#include "musicmetadata.h"

// Taglib
#include <taglib/id3v1tag.h>
#include <taglib/id3v2tag.h>
#ifdef Q_OS_WIN
// The above indirectly includes winuser.h, which unconditionally
// redefines the string SendMessage to SendMessageA.
#undef SendMessage
#endif
#include <textidentificationframe.h>
#include <attachedpictureframe.h>
#include <popularimeterframe.h>
#include <tfile.h>

// QT
#include <QList>

/*!
* \class MetaIOID3
*
* \brief Read and write metadata in MPEG (mp3) ID3V2 tags
*
* Will read ID3v1 but always writes ID3v2.4 compliant tags.
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOID3 : public MetaIOTagLib
{
  public:
    MetaIOID3(void) = default;
    ~MetaIOID3(void) override { CloseFile(); }

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIOTagLib
    bool writeVolatileMetadata(const QString &filename, MusicMetadata* mdata) override; // MetaIO

    bool writeAlbumArt(const QString &filename, const AlbumArtImage *albumart) override; // MetaIO
    bool removeAlbumArt(const QString &filename, const AlbumArtImage *albumart) override; // MetaIO

    MusicMetadata* read(const QString &filename) override; // MetaIOTagLib
    AlbumArtList getAlbumArtList(const QString &filename) override; // MetaIO
    QImage *getAlbumArt(const QString &filename, ImageType type) override; // MetaIO

    bool supportsEmbeddedImages(void) override { return true; } // MetaIO

    bool changeImageType(const QString &filename, const AlbumArtImage *albumart,
                         ImageType newType) override; // MetaIO

    bool TagExists(const QString &filename) override; // MetaIO

  private:
    bool OpenFile(const QString &filename, bool forWriting = false);
    bool SaveFile();
    void CloseFile();

    TagLib::ID3v2::Tag* GetID3v2Tag(bool create = false);
    TagLib::ID3v1::Tag* GetID3v1Tag(bool create = false);

    static bool writePlayCount(TagLib::ID3v2::Tag *tag, int playcount);
    static bool writeRating(TagLib::ID3v2::Tag *tag, int rating);
    static bool writeLastPlay(TagLib::ID3v2::Tag *tag, QDateTime lastPlay);

    static AlbumArtList readAlbumArt(TagLib::ID3v2::Tag *tag);
    static TagLib::ID3v2::UserTextIdentificationFrame* find(TagLib::ID3v2::Tag *tag,
                                      const TagLib::String &description);
    static TagLib::ID3v2::PopularimeterFrame* findPOPM(TagLib::ID3v2::Tag *tag, const TagLib::String &email);
    static TagLib::ID3v2::AttachedPictureFrame* findAPIC(TagLib::ID3v2::Tag *tag,
                                   TagLib::ID3v2::AttachedPictureFrame::Type type,
                                   const TagLib::String &description = TagLib::String());
    static QString getExtFromMimeType(const QString &mimeType);

    TagLib::File *m_file {nullptr};

    enum TagType : std::uint8_t { kMPEG, kFLAC };
    TagType m_fileType {kMPEG};
};

#endif

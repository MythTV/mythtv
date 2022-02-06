#ifndef metaioMP4_H_
#define metaioMP4_H_

// libmythmetadata
#include "metaio.h"

struct AVFormatContext;

/*!
* \class MetaIOMP4
*
* \brief Read and write metadata in MP4 container tags
*
* \copydetails MetaIO
*/
class META_PUBLIC MetaIOMP4 : public MetaIO
{
  public:
    MetaIOMP4(void) = default;
    ~MetaIOMP4(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIO
    MusicMetadata* read(const QString &filename) override; // MetaIO

  private:
    std::chrono::milliseconds getTrackLength(const QString &filename) override; // MetaIO
    static QString getFieldValue(AVFormatContext* context, const char* tagname);
    static void metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre);
};

#endif



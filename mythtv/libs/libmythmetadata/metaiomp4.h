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
    MetaIOMP4(void);
    virtual ~MetaIOMP4(void);

    bool write(MusicMetadata* mdata);
    MusicMetadata* read(const QString &filename);

  private:
    int getTrackLength(const QString &filename);
    int getTrackLength(AVFormatContext* p_context);
    QString getFieldValue(AVFormatContext* context, const char* tagname);
    void metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre);
};

#endif



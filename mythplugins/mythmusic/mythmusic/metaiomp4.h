#ifndef metaioMP4_H_
#define metaioMP4_H_

// Mythmusic
#include "metaio.h"

class AVFormatContext;

/*!
* \class MetaIOMP4
*
* \brief Read and write metadata in MP4 container tags
*
* \copydetails MetaIO
*/
class MetaIOMP4 : public MetaIO
{
  public:
    MetaIOMP4(void);
    virtual ~MetaIOMP4(void);

    bool write(Metadata* mdata);
    Metadata* read(QString filename);

  private:
    int getTrackLength(QString filename);
    int getTrackLength(AVFormatContext* p_context);
    QString getFieldValue(AVFormatContext* context, const char* tagname);
    void metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre);
};

#endif



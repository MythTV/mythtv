#ifndef metaioMP4_H_
#define metaioMP4_H_

#include "metaio.h"

extern "C" {
#include <mp4ff.h>
}

// Similar to the ones define in id3tag.h
//#define MYTH_ID3_FRAME_ALBUMARTIST "TPE4"


class MetaIOMP4 : public MetaIO
{
public:
    MetaIOMP4(void);
    virtual ~MetaIOMP4(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:

    mp4ff_callback_t *mp4_cb;
    int getTrackLength(QString filename);
    int getAACTrack(mp4ff_t *infile);
    void metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre);
};

typedef struct
{
  FILE* file;
  int fd; //only used for truncating/writing.
} mp4callback_data_t;

#endif



#ifndef metaioID3V2_H_
#define metaioID3V2_H_

#include "metaio.h"

extern "C" {
#include <id3tag.h>
}

// Similar to the ones define in id3tag.h
#define MYTH_ID3_FRAME_ALBUMARTIST "TPE4"


class MetaIOID3v2 : public MetaIO
{
public:
    MetaIOID3v2(void);
    virtual ~MetaIOID3v2(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);

    void removeComment(id3_tag *pTag, const char* pLabel);
    QString getComment(id3_tag *pTag, const char* pLabel);
    bool setComment(id3_tag *pTag, const char* pLabel, const QString& rData);
};

#endif


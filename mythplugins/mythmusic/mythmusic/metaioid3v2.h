#ifndef metaioID3V2_H_
#define metaioID3V2_H_

#include "metaio.h"

extern "C" {
#include <id3tag.h>
}

// Similar to the ones define in id3tag.h
#define MYTH_ID3_FRAME_COMPILATIONARTIST "TPE4"
#define MYTH_ID3_FRAME_COMMENT "TXXX"
#define MYTH_ID3_FRAME_MUSICBRAINZ_ALBUMARTISTDESC "MusicBrainz Album Artist Id"

class MetaIOID3v2 : public MetaIO
{
public:
    MetaIOID3v2(void);
    virtual ~MetaIOID3v2(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);

    QString getRawID3String(union id3_field *pField);
    void removeComment(id3_tag *pTag, const char* pLabel, const QString desc = "");
    QString getComment(id3_tag *pTag, const char* pLabel,
                       const QString desc = "");
    
    bool setComment(id3_tag *pTag, const char* pLabel, const QString value,
                    const QString desc = "");
};

#endif


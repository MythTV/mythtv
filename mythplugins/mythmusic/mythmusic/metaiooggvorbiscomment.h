#ifndef METAIOVORBISCOMMENT_H_
#define METAIOVORBISCOMMENT_H_

#include "metaio.h"

class OggVorbis_File;
class vorbis_comment;

class MetaIOOggVorbisComment : public MetaIO
{
public:
    MetaIOOggVorbisComment(void);
    virtual ~MetaIOOggVorbisComment(void);
    
    static vorbis_comment* getRawVorbisComment(Metadata* mdata, 
                                               vorbis_comment* pComment = NULL);
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);
    int getTrackLength(OggVorbis_File* pVf);

    QString getComment(vorbis_comment* pComment, const char* pLabel);
};

#endif


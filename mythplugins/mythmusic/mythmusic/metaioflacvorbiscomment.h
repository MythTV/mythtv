#ifndef METAIOFLACVORBISCOMMENT_H_
#define METAIOFLACVORBISCOMMENT_H_

#include "metaio.h"

#define HAVE_INTTYPES_H
#include <FLAC/all.h>

class MetaIOFLACVorbisComment : public MetaIO
{
public:
    MetaIOFLACVorbisComment(void);
    virtual ~MetaIOFLACVorbisComment(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);
    int getTrackLength(FLAC__StreamMetadata* pBlock);

    QString getComment(FLAC__StreamMetadata* pBlock, const char* pLabel);
    void setComment(FLAC__StreamMetadata* pBlock, const char* pLabel,
                    const QString& rData);
};

#endif


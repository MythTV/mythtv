#ifndef METAIOFLACVORBISCOMMENT_H_
#define METAIOFLACVORBISCOMMENT_H_

#include "metaio.h"

#include <FLAC/all.h>
// No need to include all the Flac stuff just for the abstract pointer....
//class FLAC__StreamMetadata;

class MetaIOFLACVorbisComment : public MetaIO
{
public:
    MetaIOFLACVorbisComment(void);
    virtual ~MetaIOFLACVorbisComment(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);

    QString getComment(FLAC__StreamMetadata* pBlock, const char* pLabel);
    void setComment(FLAC__StreamMetadata* pBlock, const char* pLabel,
                    const QString& rData);
};

#endif


#ifndef METAIOAVFCOMMENT_H_
#define METAIOAVFCOMMENT_H_

#include "metaio.h"

class AVFormatContext;

class MetaIOAVFComment : public MetaIO
{
public:
    MetaIOAVFComment(void);
    virtual ~MetaIOAVFComment(void);
    
    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    
private:
    int getTrackLength(QString filename);
    int getTrackLength(AVFormatContext* p_context);
};

#endif


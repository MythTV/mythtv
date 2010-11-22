#ifndef METAIOAVFCOMMENT_H_
#define METAIOAVFCOMMENT_H_

// Mythmusic
#include "metaio.h"

class AVFormatContext;

/*!
* \class MetaIOAVFComment
*
* \brief Attempt to read metadata in files without a specific metadata
*        reading implementation.
*
* N.B. No write support
*
* \copydetails MetaIO
*/
class MetaIOAVFComment : public MetaIO
{
public:
    MetaIOAVFComment(void);
    virtual ~MetaIOAVFComment(void);

    bool write(Metadata* mdata);
    Metadata* read(QString filename);

private:
    int getTrackLength(QString filename);
    int getTrackLength(AVFormatContext* p_context);
};

#endif


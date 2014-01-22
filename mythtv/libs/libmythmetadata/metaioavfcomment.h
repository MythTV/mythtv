#ifndef METAIOAVFCOMMENT_H_
#define METAIOAVFCOMMENT_H_

// libmythmetadata
#include "metaio.h"

struct AVFormatContext;

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
class META_PUBLIC MetaIOAVFComment : public MetaIO
{
public:
    MetaIOAVFComment(void);
    virtual ~MetaIOAVFComment(void);

    bool write(MusicMetadata* mdata);
    MusicMetadata* read(const QString &filename);

private:
    int getTrackLength(const QString &filename);
    int getTrackLength(AVFormatContext* p_context);
};

#endif


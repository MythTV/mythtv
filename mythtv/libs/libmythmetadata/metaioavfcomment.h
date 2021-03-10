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
    MetaIOAVFComment(void) = default;
    ~MetaIOAVFComment(void) override = default;

    bool write(const QString &filename, MusicMetadata* mdata) override; // MetaIO
    MusicMetadata* read(const QString &filename) override; // MetaIO

private:
    std::chrono::milliseconds getTrackLength(const QString &filename) override; // MetaIO
    static std::chrono::milliseconds getTrackLength(AVFormatContext* pContext);
};

#endif


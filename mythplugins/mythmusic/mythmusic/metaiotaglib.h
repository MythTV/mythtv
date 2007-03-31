#ifndef METAIOTAGLIB_H_
#define METAIOTAGLIB_H_

#include "metaio.h"
#include <id3v2tag.h>
#include <textidentificationframe.h>
#include <mpegfile.h>

using TagLib::MPEG::File;
using TagLib::Tag;
using TagLib::ID3v2::UserTextIdentificationFrame;
using TagLib::String;

class MetaIOTagLib : public MetaIO
{
public:
    MetaIOTagLib(void);
    virtual ~MetaIOTagLib(void);

    bool write(Metadata* mdata, bool exclusive = false);
    Metadata* read(QString filename);
    UserTextIdentificationFrame* find(TagLib::ID3v2::Tag *tag, const String &description);

private:

     int getTrackLength(QString filename);

//     bool setPopularimeter(id3_tag *pTag, const QString email, const int rating,
//                     const QString playcount);
};

#endif

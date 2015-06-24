//! \file
//! \brief Reads metadata (Exif & video tags) from an image file

#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QCoreApplication>
#include <QMap>
#include <QPair>

#include <mythmetaexp.h>

// FFMPEG Metadata
extern "C" {
#include <libavformat/avformat.h>
}

#include "imageutils.h"


// Exif 2.2 standard tag names, see http://www.exiv2.org/tags.html
#define EXIF_TAG_ORIENTATION      "Exif.Image.Orientation"
#define EXIF_TAG_DATETIME         "Exif.Image.DateTime"
#define EXIF_TAG_IMAGEDESCRIPTION "Exif.Image.ImageDescription"
#define EXIF_TAG_USERCOMMENT      "Exif.Photo.UserComment"


enum ImageFileTransform {
    kResetExif      = 0, //!< Reset to Exif value
    kRotateCW       = 1, //!< Rotate clockwise
    kRotateCCW      = 2, //!< Rotate anti-clockwise
    kFlipHorizontal = 3, //!< Reflect about vertical axis
    kFlipVertical   = 4  //!< Reflect about horizontal axis
};


//! Manages Exif orientation code
class META_PUBLIC ExifOrientation
{
    Q_DECLARE_TR_FUNCTIONS(ExifOrientation)
public:
    static QString FromRotation(QString);
    static QString Description(QString);
    static int     Transformed(int, int);
};


//! Reads metadata from image files
class META_PUBLIC ImageMetaData
{
public:
    typedef QPair<QString, QString> TagPair;
    typedef QMap<QString, TagPair>  TagMap;

    static bool PopulateMetaValues(ImageItem *);
    static bool GetMetaData(ImageItem *, TagMap &);

private:
    static bool ReadExifTags(QString, TagMap &);
    static bool ReadVideoTags(QString, TagMap &);
    static void ExtractVideoTags(TagMap &tags, int &arbKey, AVDictionary *dict);
};

#endif // IMAGEMETADATA_H

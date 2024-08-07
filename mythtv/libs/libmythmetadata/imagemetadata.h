//! \file
//! \brief Handles Exif/FFMpeg metadata tags for images
//! \details For pictures, Exif tags are read using libexiv2 on demand
//! For videos, tags are requested from mythffprobe
//! Common tags (used by Gallery) are Orientation, Image Comment & Capture timestamp;
//! all others are for information only. Videos have no comments; their orientation
//! & timestamp are mutated into Exif format.

#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <utility>

// Qt headers
#include <QCoreApplication> // for tr()
#include <QDateTime>
#include <QStringBuilder>
#include <QStringList>

// MythTV headers
#include "mythmetaexp.h"


// Exif 2.3 standard tag names, see http://www.exiv2.org/tags.html
static constexpr const char* EXIF_TAG_ORIENTATION      { "Exif.Image.Orientation" };
static constexpr const char* EXIF_TAG_DATETIME         { "Exif.Image.DateTime" };
static constexpr const char* EXIF_TAG_DATE_FORMAT      { "yyyy:MM:dd hh:mm:ss" };
static constexpr const char* EXIF_TAG_IMAGEDESCRIPTION { "Exif.Image.ImageDescription" };
static constexpr const char* EXIF_TAG_USERCOMMENT      { "Exif.Photo.UserComment" };
static constexpr uint16_t EXIF_PRINT_IMAGE_MATCHING    { 0xc4a5 };

// ffmpeg video tags
static constexpr const char* FFMPEG_TAG_ORIENTATION    { "rotate" };
static constexpr const char* FFMPEG_TAG_DATETIME       { "creation_time" };
static constexpr const char* FFMPEG_TAG_DATE_FORMAT    { "yyyy-MM-dd hh:mm:ss" };

// Pseudo keys for passing Myth data as metadata tags.
static constexpr const char* EXIF_MYTH_HOST   { "Myth.host" };
static constexpr const char* EXIF_MYTH_PATH   { "Myth.path" };
static constexpr const char* EXIF_MYTH_NAME   { "Myth.name" };
static constexpr const char* EXIF_MYTH_SIZE   { "Myth.size" };
static constexpr const char* EXIF_MYTH_ORIENT { "Myth.orient" };


//! Image transformations
enum ImageFileTransform : std::uint8_t {
    kResetToExif    = 0, //!< Reset to Exif value
    kRotateCW       = 1, //!< Rotate clockwise
    kRotateCCW      = 2, //!< Rotate anti-clockwise
    kFlipHorizontal = 3, //!< Reflect about vertical axis
    kFlipVertical   = 4  //!< Reflect about horizontal axis
};


//! \brief Encapsulates Exif orientation processing
//! \details The exif code indicates how the raw image should be rotated/mirrored
//! in order to display correctly. This manipulation is expensive, so done once only.
//! User transformations are applied to the code to achieve the required effect.
//! Both file orientation and current orientation are stored in the Db (together)
//! to cope with deviant Qt versions. This composite code is 2-digits where
//! 1st = current orientation, 2nd = original file orientation
class META_PUBLIC Orientation
{
    Q_DECLARE_TR_FUNCTIONS(Orientation)
public:
    explicit Orientation(int composite)
        : m_current(composite / 10), m_file(composite % 10) {}
    Orientation(int current, int file) : m_current(current), m_file(file) {}

    //! Encode original & current orientation to a single Db field
    int Composite() const { return (m_current * 10) + m_file; }
    int Transform(int transform);
    int GetCurrent(bool compensate);
    QString Description() const;

    static int FromRotation(const QString &degrees);

private:
    static QString AsText(int orientation);

    int Apply(int transform) const;

    using Matrix = QHash<int, QHash<int, int> >;

    //! True when using Qt 5.4.1 with its deviant orientation behaviour
    static const bool krunningQt541;
    //! Orientation conversions for proper display on Qt 5.4.1
    static const Matrix kQt541_orientation;
    static Matrix InitOrientationMatrix();

    //! The orientation to use: the file orientation with user transformations applied.
    int m_current;
    //! The orientation of the raw file image, as specified by the camera.
    int m_file;
};


//! Abstract class for image metadata
class META_PUBLIC ImageMetaData
{
    Q_DECLARE_TR_FUNCTIONS(ImageMetaData);
public:
    static ImageMetaData* FromPicture(const QString &filePath);
    static ImageMetaData* FromVideo(const QString &filePath);

    virtual ~ImageMetaData() = default;

    //! Unique separator to delimit fields within a string
    static const QString kSeparator;

    //! Encodes metadata into a string as \<tag name\>\<tag label\>\<tag value\>
    static QString ToString(const QString &name, const QString &label, const QString &value)
    { return name % kSeparator % label % kSeparator % value; }

    //! Decodes metadata name, label, value from a string
    static QStringList FromString(const QString &str)
    { return str.split(kSeparator); }

    using TagMap = QMultiMap<QString, QStringList>;
    static TagMap ToMap(const QStringList &tags);

    virtual bool        IsValid()                                = 0;
    virtual QStringList GetAllTags()                             = 0;
    virtual int         GetOrientation(bool *exists = nullptr)      = 0;
    virtual QDateTime   GetOriginalDateTime(bool *exists = nullptr) = 0;
    virtual QString     GetComment(bool *exists = nullptr)          = 0;

protected:
    explicit ImageMetaData(QString filePath)
        : m_filePath(std::move(filePath)) {}

    //! Image filepath
    QString m_filePath;
};


#endif // IMAGEMETADATA_H


#include "imagemetadata.h"

#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"         // for GetAppBinDir
#include "libmythbase/mythsystemlegacy.h" // for ffprobe

// libexiv2 for Exif metadata
#include <exiv2/exiv2.hpp>

// To read FFMPEG Metadata
extern "C" {
#include "libavformat/avformat.h"
}

// Uncomment this to log raw metadata from exif/ffmpeg
//#define DUMP_METADATA_TAGS yes

#define LOC QString("ImageMetaData: ")


/*!
 * \brief Adjust orientation to apply a transform to an image
 * \param transform Rotation/flip/reset to apply
 * \return int Adjusted composite orientation of the image
 */
int Orientation::Transform(int transform)
{
    m_current = Apply(transform);
    return Composite();
}


/*!
   \brief Initialises conversion matrix for Qt 5.4.1
   \return Matrix of orientation codes such that:
    Qt 5.4.1 orientation = matrix(file orientation, current orientation)
 */
Orientation::Matrix Orientation::InitOrientationMatrix()
{
    Orientation::Matrix matrix;

    if (krunningQt541)
    {
        // Each row/string defines codes for a single file orientation
        // Each col/value defines applicable code for corresponding current orientation
        // As current orientation is applicable to raw camera image, these codes
        // define the current orientation relative to 1/Normal (as Qt 5.4.1 has already
        // applied the file orientation)
        QStringList vals = QStringList()
                << "0 1 2 3 4 5 6 7 8"
                << "0 1 2 3 4 5 6 7 8"
                << "0 2 1 4 3 8 7 6 5"
                << "0 3 4 1 2 7 8 5 6"
                << "0 4 3 2 1 6 5 8 7"
                << "0 5 6 7 8 1 2 3 4"
                << "0 8 7 6 5 2 1 4 3"
                << "0 7 8 5 6 3 4 1 2"
                << "0 6 5 8 7 4 3 2 1";

        for (int row = 0; row < vals.size(); ++row)
        {
            QStringList rowVals = vals.at(row).split(' ');
            for (int col = 0; col < rowVals.size(); ++col)
                matrix[row][col] = rowVals.at(col).toInt();
        }
    }
    return matrix;
}

const bool Orientation::krunningQt541 = (strcmp(qVersion(), "5.4.1") == 0);
const Orientation::Matrix Orientation::kQt541_orientation =
        Orientation::InitOrientationMatrix();


/*!
 * \brief Determines orientation required for an image
 * \details Some Qt versions automatically apply file orientation when an image
 * is loaded. This compensates for that to ensure images are always orientated
   correctly.
 * \param compensate Whether to compensate for Qt auto-rotation
 * \return Exif orientation code to apply after the image has been loaded.
 */
int Orientation::GetCurrent(bool compensate)
{
    // Qt 5.4.1 automatically applies the file orientation when loading images
    // Ref: https://codereview.qt-project.org/#/c/111398/
    // Ref: https://codereview.qt-project.org/#/c/110685/
    // https://bugreports.qt.io/browse/QTBUG-37946
    if (compensate && krunningQt541)
    {
        // Deduce orientation relative to 1/Normal from file & current orientations
        int old = m_current;
        m_current = kQt541_orientation.value(m_file).value(m_current);

        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Adjusted orientation %1 to %2 for Qt 5.4.1")
            .arg(old).arg(m_current));
    }
    return m_current;
}


/*!
 * \brief Adjust current orientation code to apply a transform to an image
 * \details When displayed the image will be orientated iaw its orientation
 * code. The transform is effected by applying the reverse transform to the
 * orientation code.
 * \sa http://jpegclub.org/exif_orientation.html
 * \param transform Rotation/flip to apply
 * \return int New orientation code that will apply the transform to the image
 */
int Orientation::Apply(int transform) const
{
    if (transform == kResetToExif)
        return m_file;

    // https://github.com/recurser/exif-orientation-examples is a useful resource.
    switch (m_current)
    {
    case 0: // The image has no orientation info
    case 1: // The image is in its original state
        switch (transform)
        {
        case kRotateCW:       return 6;
        case kRotateCCW:      return 8;
        case kFlipHorizontal: return 2;
        case kFlipVertical:   return 4;
        }
        break;

    case 2: // The image is horizontally flipped
        switch (transform)
        {
        case kRotateCW:       return 7;
        case kRotateCCW:      return 5;
        case kFlipHorizontal: return 1;
        case kFlipVertical:   return 3;
        }
        break;

    case 3: // The image is rotated 180°
        switch (transform)
        {
        case kRotateCW:       return 8;
        case kRotateCCW:      return 6;
        case kFlipHorizontal: return 4;
        case kFlipVertical:   return 2;
        }
        break;

    case 4: // The image is vertically flipped
        switch (transform)
        {
        case kRotateCW:       return 5;
        case kRotateCCW:      return 7;
        case kFlipHorizontal: return 3;
        case kFlipVertical:   return 1;
        }
        break;

    case 5: // The image is rotated 90° CW and flipped horizontally
        switch (transform)
        {
        case kRotateCW:       return 2;
        case kRotateCCW:      return 4;
        case kFlipHorizontal: return 6;
        case kFlipVertical:   return 8;
        }
        break;

    case 6: // The image is rotated 90° CCW
        switch (transform)
        {
        case kRotateCW:       return 3;
        case kRotateCCW:      return 1;
        case kFlipHorizontal: return 5;
        case kFlipVertical:   return 7;
        }
        break;

    case 7: // The image is rotated 90° CW and flipped vertically
        switch (transform)
        {
        case kRotateCW:       return 4;
        case kRotateCCW:      return 2;
        case kFlipHorizontal: return 8;
        case kFlipVertical:   return 6;
        }
        break;

    case 8: // The image is rotated 90° CW
        switch (transform)
        {
        case kRotateCW:       return 1;
        case kRotateCCW:      return 3;
        case kFlipHorizontal: return 7;
        case kFlipVertical:   return 5;
        }
        break;
    }
    return m_current;
}


/*!
 \brief Convert degrees of rotation into Exif orientation code
 \param degrees CW rotation required to show video correctly
 \return QString Orientation code as per Exif spec.
*/
int Orientation::FromRotation(const QString &degrees)
{
    if (degrees ==   "0") return 1;
    if (degrees ==  "90") return 6;
    if (degrees == "180") return 3;
    if (degrees == "270") return 8;
    return 0;
}


/*!
 * \brief Generate text description of orientation
 * \details Reports code & its interpretation of file orientation and, if
 * different, the Db orientation
 * \return Text description of orientation
 */
QString Orientation::Description() const
{
    return (m_file == m_current)
            ? AsText(m_file)
            : tr("File: %1, Db: %2").arg(AsText(m_file),
                                         AsText(m_current));
}


/*!
 \brief Converts orientation code to text description for info display
 \param orientation Exif code
 \return QString Description text
*/
QString Orientation::AsText(int orientation)
{
    switch (orientation)
    {
    case 1:  return tr("1 (Normal)");
    case 2:  return tr("2 (H Mirror)");
    case 3:  return tr("3 (Rotate 180°)");
    case 4:  return tr("4 (V Mirror)");
    case 5:  return tr("5 (H Mirror, Rot 270°)");
    case 6:  return tr("6 (Rotate 90°)");
    case 7:  return tr("7 (H Mirror, Rot 90°)");
    case 8:  return tr("8 (Rotate 270°)");
    default: return tr("%1 (Undefined)").arg(orientation);
    }
}


//! Reads Exif metadata from a picture using libexiv2
class PictureMetaData : public ImageMetaData
{
public:
    explicit PictureMetaData(const QString &filePath);
    ~PictureMetaData() override = default; // libexiv2 closes file, cleans up via autoptrs

    bool        IsValid() override // ImageMetaData
        { return m_image.get(); }
    QStringList GetAllTags() override; // ImageMetaData
    int         GetOrientation(bool *exists = nullptr) override; // ImageMetaData
    QDateTime   GetOriginalDateTime(bool *exists = nullptr) override; // ImageMetaData
    QString     GetComment(bool *exists = nullptr) override; // ImageMetaData

protected:
    static QString DecodeComment(std::string rawValue);

    std::string GetTag(const QString &key, bool *exists = nullptr);

    Exiv2::Image::UniquePtr m_image;
    Exiv2::ExifData       m_exifData;
};


/*!
   \brief Constructor. Reads metadata from image.
   \param filePath Absolute image path
 */
PictureMetaData::PictureMetaData(const QString &filePath)
    : ImageMetaData(filePath), m_image(nullptr)
{
    try
    {
        m_image = Exiv2::ImageFactory::open(filePath.toStdString());

        if (PictureMetaData::IsValid())
        {
            m_image->readMetadata();
            m_exifData = m_image->exifData();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Exiv2 error: Could not open file %1").arg(filePath));
        }
    }
    catch (Exiv2::Error &e)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Exiv2 exception %1").arg(e.what()));
    }
}


/*!
   \brief Returns all metadata tags
   \details Ignores "Exif.Image.PrintImageMatching" and lengthy tag values,
   which are probably proprietary/binary
   \return List of encoded strings
   \sa ImageMetaData::FromString()
 */
QStringList PictureMetaData::GetAllTags()
{
    QStringList tags;
    if (!IsValid())
        return tags;

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Found %1 tag(s) for file %2")
        .arg(m_exifData.count()).arg(m_filePath));

    Exiv2::ExifData::const_iterator i;
    for (i = m_exifData.begin(); i != m_exifData.end(); ++i)
    {
        QString label = QString::fromStdString(i->tagLabel());

        // Ignore empty labels
        if (label.isEmpty())
            continue;

        QString key = QString::fromStdString(i->key());

        // Ignore large values (binary/private tags)
        if (i->size() >= 256)
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("Ignoring %1 (%2, %3) : Too big")
                .arg(key, i->typeName()).arg(i->size()));
        }
        // Ignore 'Print Image Matching'
        else if (i->tag() == EXIF_PRINT_IMAGE_MATCHING)
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("Ignoring %1 (%2, %3) : Undecodable")
                .arg(key, i->typeName()).arg(i->size()));
        }
        else
        {
            // Use interpreted values
            std::string val = i->print(&m_exifData);

            // Comment needs charset decoding
            QString value = (key == EXIF_TAG_USERCOMMENT)
                    ? DecodeComment(val) : QString::fromStdString(val);

            // Nulls can arise from corrupt metadata (MakerNote)
            // Remove them as they disrupt socket comms between BE & remote FE's
            if (value.contains(QChar::Null))
            {
                LOG(VB_GENERAL, LOG_NOTICE, LOC +
                    QString("Corrupted Exif detected in %1").arg(m_filePath));
                value = "????";
            }

            // Encode tag
            QString str = ToString(key, label, value);
            tags << str;

#ifdef DUMP_METADATA_TAGS
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("%1 (%2, %3)")
                .arg(str, i->typeName()).arg(i->size()));
#endif
        }
    }
    return tags;
}


/*!
   \brief Read a single Exif metadata tag
   \param [in] key Exif tag key, as per http://www.exiv2.org/tags.html
   \param [out] exists (Optional) True if key is found in metadata
   \return Encoded tag
   \sa ImageMetaData::FromString()
 */
std::string PictureMetaData::GetTag(const QString &key, bool *exists)
{
    std::string value;
    if (exists)
        *exists = false;

    if (!IsValid())
        return value;

    Exiv2::ExifKey exifKey = Exiv2::ExifKey(key.toStdString());
    auto exifIt = m_exifData.findKey(exifKey);

    if (exifIt == m_exifData.end())
        return value;

    if (exists)
        *exists = true;

    // Use raw value
    return exifIt->value().toString();
}


/*!
   \brief Read Exif orientation
   \param [out] exists (Optional) True if orientation is defined by metadata
   \return Exif orientation code
 */
int PictureMetaData::GetOrientation(bool *exists)
{
    std::string value = GetTag(EXIF_TAG_ORIENTATION, exists);
    return QString::fromStdString(value).toInt();
}


/*!
   \brief Read Exif timestamp of image capture
   \param [out] exists (Optional) True if date exists in metadata
   \return Timestamp (possibly invalid) in camera timezone
 */
QDateTime PictureMetaData::GetOriginalDateTime(bool *exists)
{
    std::string value = GetTag(EXIF_TAG_DATETIME, exists);
    QString dt = QString::fromStdString(value);

    // Exif time has no timezone
    return QDateTime::fromString(dt, EXIF_TAG_DATE_FORMAT);
}


/*!
   \brief Read Exif comments from metadata
   \details Returns UserComment, if not empty. Otherwise returns ImageDescription
   \param [out] exists (Optional) True if either comment is found in metadata
   \return Comment as a string
 */
QString PictureMetaData::GetComment(bool *exists)
{
    // Use User Comment or else Image Description
    bool comExists = false;
    bool desExists = false;

    std::string comment = GetTag(EXIF_TAG_USERCOMMENT, &comExists);

    if (comment.empty())
        comment = GetTag(EXIF_TAG_IMAGEDESCRIPTION, &desExists);

    if (exists)
        *exists = comExists || desExists;

    return DecodeComment(comment);
}


/*!
   \brief Decodes charset of UserComment
   \param rawValue Metadata value with optional "[charset=...]" prefix
   \return Decoded comment
 */
QString PictureMetaData::DecodeComment(std::string rawValue)
{
    // Decode charset
    Exiv2::CommentValue comVal = Exiv2::CommentValue(rawValue);
    if (comVal.charsetId() != Exiv2::CommentValue::undefined)
        rawValue = comVal.comment();
    return QString::fromStdString(rawValue);
}


//! Reads video metadata tags using FFmpeg
//! Raw values for Orientation & Date are read quickly via FFmpeg API.
//! However, as collating and interpreting other tags is messy and dependant on
//! internal FFmpeg changes, informational data is derived via mythffprobe
//! (a slow operation)
class VideoMetaData : public ImageMetaData
{
public:
    explicit VideoMetaData(const QString &filePath);
    ~VideoMetaData() override;

    bool        IsValid() override  // ImageMetaData
        { return m_dict; }
    QStringList GetAllTags() override; // ImageMetaData
    int         GetOrientation(bool *exists = nullptr) override; // ImageMetaData
    QDateTime   GetOriginalDateTime(bool *exists = nullptr) override; // ImageMetaData
    QString     GetComment(bool *exists = nullptr) override; // ImageMetaData

protected:
    QString GetTag(const QString &key, bool *exists = nullptr);

    AVFormatContext *m_context { nullptr };
    //! FFmpeg tag dictionary
    AVDictionary    *m_dict    { nullptr };
};


/*!
   \brief Constructor. Opens best video stream from video
   \param filePath Absolute video path
 */
VideoMetaData::VideoMetaData(const QString &filePath)
    : ImageMetaData(filePath)
{
    AVInputFormat* p_inputformat = nullptr;

    // Open file
    if (avformat_open_input(&m_context, filePath.toLatin1().constData(),
                            p_inputformat, nullptr) < 0)
        return;

    // Locate video stream
    int vidStream = av_find_best_stream(m_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vidStream >= 0)
        m_dict  = m_context->streams[vidStream]->metadata;

    if (!VideoMetaData::IsValid())
        avformat_close_input(&m_context);
}


/*!
   \brief Destructor. Closes file
 */
VideoMetaData::~VideoMetaData()
{
    if (VideoMetaData::IsValid())
        avformat_close_input(&m_context);
}


/*!
   \brief Reads relevant video metadata by running mythffprobe.
   \warning Blocks for up to 5 secs
   \details As video tags are unstructured they are massaged into groups of format,
   stream0, streamN to segregate them and permit reasonable display ordering.
   The stream indices reflect the stream order returned by mythffprobe and do not
   necessarily correlate with FFmpeg streams
   \return List of encoded video metadata tags
   \sa ImageMetaData::FromString()
 */
QStringList VideoMetaData::GetAllTags()
{
    QStringList tags;
    if (!IsValid())
        return tags;

    // Only extract interesting fields:
    // For list use: mythffprobe -show_format -show_streams <file>
    QString cmd = GetAppBinDir() + MYTH_APPNAME_MYTHFFPROBE;
    QStringList args;
    args << "-loglevel quiet"
         << "-print_format compact" // Returns "section|key=value|key=value..."
         << "-pretty"               // Add units etc
         << "-show_entries "
            "format=format_long_name,duration,bit_rate:format_tags:"
            "stream=codec_long_name,codec_type,width,height,pix_fmt,color_space,avg_frame_rate"
            ",codec_tag_string,sample_rate,channels,channel_layout,bit_rate:stream_tags"
         << m_filePath;

    MythSystemLegacy ffprobe(cmd, args, kMSRunShell | kMSStdOut);

    ffprobe.Run(5s);

    if (ffprobe.Wait() != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Timeout or Failed: %2 %3").arg(cmd, args.join(" ")));
        return tags;
    }

    QByteArray result = ffprobe.ReadAll();
    QTextStream ostream(result);
    int stream = 0;
    while (!ostream.atEnd())
    {
        QStringList fields = ostream.readLine().split('|');

        if (fields.size() <= 1)
            // Empty section
            continue;

        // First fields should be "format" or "stream"
        QString prefix = "";
        QString group  = fields.takeFirst();
        if (group == "stream")
        {
            // Streams use index as group
            prefix = QString::number(stream++) + ":";
            group.append(prefix);
        }

        for (const auto& field : std::as_const(fields))
        {
            // Expect label=value
            QStringList parts = field.split('=');
            if (parts.size() != 2)
                continue;

            // Remove ffprobe "tag:" prefix
            QString label = parts[0].remove("tag:");
            QString value = parts[1];

            // Construct a pseudo-key for FFMPEG tags
            QString key = QString("FFmpeg.%1.%2").arg(group, label);

            // Add stream id to labels
            QString str = ToString(key, prefix + label, value);
            tags << str;

#ifdef DUMP_METADATA_TAGS
            LOG(VB_FILE, LOG_DEBUG, LOC + str);
#endif
        }
    }
    return tags;
}


/*!
   \brief Read a single video tag
   \param [in] key FFmpeg tag name
   \param [out] exists (Optional) True if tag exists in metadata
   \return Tag value as a string
 */
QString VideoMetaData::GetTag(const QString &key, bool *exists)
{
    if (m_dict)
    {
        AVDictionaryEntry *tag = nullptr;
        while ((tag = av_dict_get(m_dict, "\0", tag, AV_DICT_IGNORE_SUFFIX)))
        {
            if (QString(tag->key) == key)
            {
                if (exists)
                    *exists = true;
                return QString::fromUtf8(tag->value);
            }
        }
    }
    if (exists)
        *exists = false;
    return {};
}


/*!
   \brief Read FFmpeg video orientation tag
   \param [out] exists (Optional) True if orientation is defined by metadata
   \return Exif orientation code
 */
int VideoMetaData::GetOrientation(bool *exists)
{
    QString angle = GetTag(FFMPEG_TAG_ORIENTATION, exists);
    return Orientation::FromRotation(angle);
}


/*!
   \brief Read video datestamp
   \param [out] exists (Optional) True if datestamp is defined by metadata
   \return Timestamp (possibly invalid) in camera timezone
 */
QDateTime VideoMetaData::GetOriginalDateTime(bool *exists)
{
    QString dt = GetTag(FFMPEG_TAG_DATETIME, exists);

    // Video time has no timezone
    return QDateTime::fromString(dt, FFMPEG_TAG_DATE_FORMAT);
}


/*!
   \brief Read Video comment from metadata
   \details Always empty
   \param [out] exists (Optional) Always false
   \return Empty comment
 */
QString VideoMetaData::GetComment(bool *exists)
{
    if (exists)
        *exists = false;
    return {};
}


/*!
   \brief Factory to retrieve metadata from pictures
   \param filePath Image path
   \return Picture metadata reader
*/
ImageMetaData* ImageMetaData::FromPicture(const QString &filePath)
{ return new PictureMetaData(filePath); }


/*!
   \brief Factory to retrieve metadata from videos
   \param filePath Image path
   \return Video metadata reader
 */
ImageMetaData* ImageMetaData::FromVideo(const QString &filePath)
{ return new VideoMetaData(filePath); }


const QString ImageMetaData::kSeparator = "|-|";


/*!
   \brief Creates a map of metadata tags as
   \param tagStrings List of strings containing encoded metadata
   \return multimap<key group, list<key, label, value>>
 */
ImageMetaData::TagMap ImageMetaData::ToMap(const QStringList &tagStrings)
{
    TagMap tags;
    for (const auto& token : std::as_const(tagStrings))
    {
        QStringList parts = FromString(token);
        // Expect Key, Label, Value.
        if (parts.size() == 3)
        {
            // Map tags by Family.Group to keep them together
            // Within each group they will preserve list ordering
            QString group = parts[0].section('.', 0, 1);
            tags.insert(group, parts);

#ifdef DUMP_METADATA_TAGS
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("%1 = %2").arg(group, token));
#endif
        }
    }
    return tags;
}

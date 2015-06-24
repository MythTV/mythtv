#include "imagemetadata.h"

#include <QRegExp>
#include <QMap>
#include <QPair>
#include <QDateTime>
#include <QMutexLocker>
#include <QByteArray>

// libexiv2 for Exif metadata
#include <exiv2/exiv2.hpp>
// Note: Older versions of Exiv2 don't have the exiv2.hpp include
// file.  Using image.hpp instead seems to work.
#ifdef _MSC_VER
#include <exiv2/src/image.hpp>
#else
#include <exiv2/image.hpp>
#endif

#include <mythlogging.h>


/*!
 \brief Convert degree rotation into orientation code
 \param degrees Rotation required to show video correctly
 \return QString Code as per Exif spec.
*/
QString ExifOrientation::FromRotation(QString degrees)
{
    if      (degrees ==   "0") return "1";
    else if (degrees ==  "90") return "6";
    else if (degrees == "180") return "3";
    else if (degrees == "270") return "8";
    return "0";
}


/*!
 \brief Converts orientation code to text description
 \param orientation Exif code
 \return QString Description text
*/
QString ExifOrientation::Description(QString orientation)
{
    if      (orientation == "0") return tr("0 - Unset");
    else if (orientation == "1") return tr("1 - Normal");
    else if (orientation == "2") return tr("2 - Horizontally Reflected");
    else if (orientation == "3") return tr("3 - Rotated 180°");
    else if (orientation == "4") return tr("4 - Vertically Reflected");
    else if (orientation == "5") return tr("5 - Rotated 90°, Horizontally Reflected");
    else if (orientation == "6") return tr("6 - Rotated 270°");
    else if (orientation == "7") return tr("7 - Rotated 90°, Vertically Reflected");
    else if (orientation == "8") return tr("8 - Rotated 90°");
    return orientation;
}


/*!
 * \brief Determines effect of applying a transform to an image
 * \sa http://jpegclub.org/exif_orientation.html
 * \details These transforms are not intuitive!
 * For rotations the orientation is adjusted in the opposite direction.
 * The transform is applied from the user perspective (as the image will be displayed),
 * not the current orientation. When rotated 90° horizontal/vertical flips are
 * reversed, and when flipped rotations are reversed.
 * \param im Image
 * \param transform Rotation/flip
 * \return int New orientation after applying transform
 */
int ExifOrientation::Transformed(int orientation, int transform)
{
    switch (orientation)
    {
    case 0: // The image has no orientation info
    case 1: // The image is in its original state
        switch (transform)
        {
        case kRotateCW:       return 6;
        case kRotateCCW:      return 8;
        case kFlipHorizontal: return 2;
        case kFlipVertical:   return 4;
        default:              return orientation;
        }

    case 2: // The image is horizontally flipped
        switch (transform)
        {
        case kRotateCW:       return 7;
        case kRotateCCW:      return 5;
        case kFlipHorizontal: return 1;
        case kFlipVertical:   return 3;
        default:              return orientation;
        }

    case 3: // The image is rotated 180°
        switch (transform)
        {
        case kRotateCW:       return 8;
        case kRotateCCW:      return 6;
        case kFlipHorizontal: return 4;
        case kFlipVertical:   return 2;
        default:              return orientation;
        }

    case 4: // The image is vertically flipped
        switch (transform)
        {
        case kRotateCW:       return 5;
        case kRotateCCW:      return 7;
        case kFlipHorizontal: return 3;
        case kFlipVertical:   return 1;
        default:              return orientation;
        }

    case 5: // The image is transposed (rotated 90° CW flipped horizontally)
        switch (transform)
        {
        case kRotateCW:       return 2;
        case kRotateCCW:      return 4;
        case kFlipHorizontal: return 6;
        case kFlipVertical:   return 8;
        default:              return orientation;
        }

    case 6: // The image is rotated 90° CCW
        switch (transform)
        {
        case kRotateCW:       return 3;
        case kRotateCCW:      return 1;
        case kFlipHorizontal: return 5;
        case kFlipVertical:   return 7;
        default:              return orientation;
        }

    case 7: // The image is transversed  (rotated 90° CW and flipped
        // vertically)
        switch (transform)
        {
        case kRotateCW:       return 4;
        case kRotateCCW:      return 2;
        case kFlipHorizontal: return 8;
        case kFlipVertical:   return 6;
        default:              return orientation;
        }

    case 8: // The image is rotated 90° CW
        switch (transform)
        {
        case kRotateCW:       return 1;
        case kRotateCCW:      return 3;
        case kFlipHorizontal: return 7;
        case kFlipVertical:   return 5;
        default:              return orientation;
        }

    default: return orientation;
    }
}


/*!
 \brief Sets orientation, datestamp & comment from file metadata
 \details Reads Exif for pictures, metadata tags from FFMPEG for videos
 \param im The image item to set
 \return bool True if metadata found
*/
bool ImageMetaData::PopulateMetaValues(ImageItem *im)
{
    QString absPath = ImageSg::getInstance()->GetFilePath(im);
    TagMap tags;

    // Only require orientation, date, comment
    QPair<QString, QString> toBeFilled = qMakePair(QString(), QString());
    tags.insert(EXIF_TAG_ORIENTATION, toBeFilled);
    tags.insert(EXIF_TAG_DATETIME, toBeFilled);
    tags.insert(EXIF_TAG_USERCOMMENT, toBeFilled);
    tags.insert(EXIF_TAG_IMAGEDESCRIPTION, toBeFilled);

    bool ok = false;
    if (im->m_type == kImageFile)
    {
        ok = ReadExifTags(absPath, tags);
    }
    else if (im->m_type == kVideoFile)
    {
        ok = ReadVideoTags(absPath, tags);
    }

    if (ok)
    {
        // Extract orientation
        if (tags.contains(EXIF_TAG_ORIENTATION))
        {
            QString orient = tags.value(EXIF_TAG_ORIENTATION).first;
            bool valid;
            int orientation = orient.toInt(&valid);
            im->m_orientation = (valid ? orientation : 0);
        }
        else
        {
            im->m_orientation = 0;
            LOG(VB_FILE, LOG_DEBUG,
                QString("Image: No Orientation metadata in %1").arg(im->m_name));
        }

        // Extract Datetime
        if (tags.contains(EXIF_TAG_DATETIME))
        {
            QString date = tags.value(EXIF_TAG_DATETIME).first;
            // Exif time has no timezone
            QDateTime dateTime = QDateTime::fromString(date, "yyyy:MM:dd hh:mm:ss");
            if (dateTime.isValid())
                im->m_date = dateTime.toTime_t();
        }
        else
        {
            im->m_date = 0;
            LOG(VB_FILE, LOG_DEBUG,
                QString("Image: No DateStamp metadata in %1").arg(im->m_name));
        }

        // Extract User Comment or else Image Description
        QString comment = "";
        if (tags.contains(EXIF_TAG_USERCOMMENT))
        {
            comment = tags.value(EXIF_TAG_USERCOMMENT).first;
        }
        else if (tags.contains(EXIF_TAG_IMAGEDESCRIPTION))
        {
            comment = tags.value(EXIF_TAG_IMAGEDESCRIPTION).first;
        }
        else
        {
            LOG(VB_FILE, LOG_DEBUG, QString("Image: No Comment metadata in %1")
                .arg(im->m_name));
        }
        im->m_comment = comment.simplified();
    }

    return ok;
}


/*!
 \brief Reads all metadata for an image
 \param im The image
 \param tags Map of metadata tags. Map values = Pair< camera value, camera tag label >
For pictures: key = Exif standard tag name; for videos: key = Exif tag name for
orientation, date & comment only and an arbitrary, unique int for all other tags.
 \return bool True if metadata exists & could be read
*/
bool ImageMetaData::GetMetaData(ImageItem *im, TagMap &tags)
{
    QString absPath = ImageSg::getInstance()->GetFilePath(im);

    if (absPath.isEmpty())
        return false;

    //
    bool ok = false;
    if (im->m_type == kImageFile)
    {
        ok = ReadExifTags(absPath, tags);
    }
    else if (im->m_type == kVideoFile)
    {
        ok = ReadVideoTags(absPath, tags);
    }

    if (ok && tags.contains(EXIF_TAG_ORIENTATION))
    {
        TagPair val = tags.value(EXIF_TAG_ORIENTATION);
        tags.insert(EXIF_TAG_ORIENTATION,
                    qMakePair(ExifOrientation::Description(val.first), val.second));
    }
    return ok;
}


/*!
 \brief Get Exif tags for an image
 \param filePath Image file
 \param exif Map of exif tags & values requested and/or returned.
 For each key the corresponding exif value is populated.
 If empty it is populated with all exif key/values from the image.
 \return bool False on exif error
*/
bool ImageMetaData::ReadExifTags(QString filePath, TagMap &tags)
{
    try
    {
        Exiv2::Image::AutoPtr image =
            Exiv2::ImageFactory::open(filePath.toLocal8Bit().constData());

        if (!image.get())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Image: Exiv2 error: Could not open file %1").arg(
                    filePath));
            return false;
        }

        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();

        if (exifData.empty())
        {
            LOG(VB_FILE, LOG_NOTICE,
                QString("Image: Exiv2 error: No exif data for file %1").arg(filePath));
            return false;
        }

        if (tags.isEmpty())
        {
            // No specific tags requested - extract all exif data
            LOG(VB_FILE, LOG_DEBUG,
                QString("Image: Found %1 tag(s) for file %2")
                .arg(exifData.count())
                .arg(filePath));

            Exiv2::ExifData::const_iterator i;
            for (i = exifData.begin(); i != exifData.end(); ++i)
            {
                QString label = QString::fromStdString(i->tagLabel());

                // Ignore empty labels
                if (!label.isEmpty())
                {
                    QString key   = QString::fromStdString(i->key());
                    std::string rawValue = i->value().toString();
                    QString value;

                    if (key == EXIF_TAG_USERCOMMENT)
                    {
                        // Decode charset
                        Exiv2::CommentValue comVal = Exiv2::CommentValue(rawValue);
                        value = QString::fromStdString(comVal.comment());
                    }
                    else
                        value = QString::fromStdString(rawValue);

                    // Remove control chars from malformed exif values.
                    // They can pervert the myth message response mechanism
                    value.replace(QRegExp("[\\0000-\\0037]"), "");

#if 0
                    LOG(VB_FILE, LOG_DEBUG,
                        QString("Image: Exif %1/\"%2\" (Type %3) : %4")
                        .arg(key, label, i->typeName(), value));
#endif
                    tags.insert(key, qMakePair(value, label));
                }
            }
        }
        else
        {
            // Extract requested tags only
            QMap<QString, QPair<QString, QString> >::iterator it;
            for (it = tags.begin(); it != tags.end(); ++it)
            {
                Exiv2::ExifKey            key =
                        Exiv2::ExifKey(it.key().toLocal8Bit().constData());
                Exiv2::ExifData::iterator exifIt = exifData.findKey(key);

                if (exifIt != exifData.end())
                {
                    QString value;
                    std::string rawValue = exifIt->value().toString();
                    if (key.key() == EXIF_TAG_USERCOMMENT)
                    {
                        // Decode charset
                        Exiv2::CommentValue comVal = Exiv2::CommentValue(rawValue);
                        value = QString::fromStdString(comVal.comment());
                    }
                    else
                        value = QString::fromStdString(rawValue);

                    it.value() = qMakePair(value, QString());
                }
            }
        }
        return true;
    }
    catch (Exiv2::Error &e)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Image: Exiv2 exception %1").arg(e.what()));
    }
    return false;
}


/*!
 \brief Extract metadata tags from FFMPEG dict
 \param[in,out] tags Extracted tags
 \param[in,out] arbKey Used as a map key for tags other than Orientation or Date
 \param dict    FFMPEG metdata dict containing video metadata
*/
void ImageMetaData::ExtractVideoTags(TagMap &tags, int &arbKey, AVDictionary *dict)
{
    AVDictionaryEntry *avTag = av_dict_get(dict, "\0", NULL, AV_DICT_IGNORE_SUFFIX);
    while (avTag)
    {
        QString key;
        QString label = QString(avTag->key);
        QString value = QString::fromUtf8(avTag->value);

        if (label == "rotate")
        {
            // Flag orientation & convert to Exif code
            key   = EXIF_TAG_ORIENTATION;
            label = "Orientation";
            value = ExifOrientation::FromRotation(value);
        }
        else if (label == "creation_time")
        {
            // Flag date & convert to Exif date format "YYYY:MM:DD"
            key   = EXIF_TAG_DATETIME;
            label = "Date and Time";
            value.replace("-", ":");
        }
        else
            key = QString::number(arbKey++);

        tags.insert(key, qMakePair(value, label));
#if 0
        LOG(VB_FILE, LOG_DEBUG,
            QString("Image: Video %1/\"%2\" : %3").arg(key, avTag->key, value));
#endif
        avTag = av_dict_get(dict, "\0", avTag, AV_DICT_IGNORE_SUFFIX);
    }
}


/*!
 \brief Get metadata tags from a video file
 \param filePath Video file
 \param[in,out] tags Map of extracted tags
 \return bool True if metadata exists and could be read
*/
bool ImageMetaData::ReadVideoTags(QString filePath, TagMap &tags)
{
    {
        QMutexLocker locker(avcodeclock);
        av_register_all();
    }

    AVFormatContext* p_context = NULL;
    AVInputFormat* p_inputformat = NULL;
    QByteArray local8bit = filePath.toLocal8Bit();

    // Open file
    if ((avformat_open_input(&p_context, local8bit.constData(),
                             p_inputformat, NULL) < 0))
        return false;

    // Locate video stream
    int vidStream = av_find_best_stream(p_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vidStream < 0)
        return false;

    // Cannot search tags so must extract them all
    // No tag classification for video so use arbitrary, unique keys

    int arbKey = 1;
    // Extract file tags
    ExtractVideoTags(tags, arbKey, p_context->metadata);
    // Extract video tags
    ExtractVideoTags(tags, arbKey, p_context->streams[vidStream]->metadata);

    avformat_close_input(&p_context);

    return true;
}

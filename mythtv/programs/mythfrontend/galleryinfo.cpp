// C++
#include <chrono>

// MythTV
#include "libmythbase/filesysteminfo.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythmetadata/imagemetadata.h"

// MythFrontend
#include "galleryinfo.h"


//! The exif/video tags comprising the Basic file info
static QSet<QString> kBasicInfoSet {
        // Exif tags
        EXIF_TAG_USERCOMMENT,
        EXIF_TAG_IMAGEDESCRIPTION,
        EXIF_TAG_ORIENTATION,
        EXIF_TAG_DATETIME,
        "Exif.Image.Make",
        "Exif.Image.Model",
        "Exif.Photo.ExposureTime",
        "Exif.Photo.ShutterSpeedValue",
        "Exif.Photo.FNumber",
        "Exif.Photo.ApertureValue",
        "Exif.Photo.ExposureBiasValue",
        "Exif.Photo.Flash",
        "Exif.Photo.FocalLength",
        "Exif.Photo.FocalLengthIn35mmFilm",
        "ISO speed",
        "Exif.Photo.MeteringMode",
        "Exif.Photo.PixelXDimension",
        "Exif.Photo.PixelYDimension",
           // Video tags
        "FFmpeg.format.format_long_name",
        "FFmpeg.format.duration",
        "FFmpeg.format.creation_time",
        "FFmpeg.format.model",
        "FFmpeg.format.make",
           // Only detects tags within the first 2 streams for efficiency
        "FFmpeg.stream0:.codec_long_name",
        "FFmpeg.stream1:.codec_long_name",
        "FFmpeg.stream0:.width",
        "FFmpeg.stream1:.width",
        "FFmpeg.stream0:.height",
        "FFmpeg.stream1:.height",
        "FFmpeg.stream0:.sample_rate",
        "FFmpeg.stream1:.sample_rate",
        "FFmpeg.stream0:.rotate",
        "FFmpeg.stream1:.rotate" };

//! Constructor
InfoList::InfoList(MythScreenType &screen)
    : m_screen(screen), m_mgr(ImageManagerFe::getInstance())
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(1s);
    connect(&m_timer, &QTimer::timeout, this, &InfoList::Clear);
}


/*!
 \brief Initialise buttonlist from XML
 \param focusable Set if info list should be focusable (for scrolling)
 \return bool True initialisation succeeds
*/
bool InfoList::Create(bool focusable)
{
    bool err = false;
    UIUtilE::Assign(&m_screen, m_btnList, "infolist", &err);
    if (err)
        return false;

    m_btnList->SetVisible(false);
    m_btnList->SetCanTakeFocus(focusable);
    return true;
}


/*!
 \brief Toggle infolist state for an image. Focusable widgets toggle between
 Basic & Full info. Non-focusable widgets toggle between Basic & Off.
 \param im The image/dir for which info is shown
*/
void InfoList::Toggle(const ImagePtrK& im)
{
    if (!im)
        return;

    // Only focusable lists have an extra 'full' state as they can
    // be scrolled to view it all
    if (m_btnList->CanTakeFocus())
    {
        // Start showing basic info then toggle between basic/full
        m_infoVisible = m_infoVisible == kBasicInfo ? kFullInfo : kBasicInfo;

    // Toggle between off/basic
    }
    else if (m_infoVisible == kBasicInfo)
    {
        m_infoVisible = kNoInfo;
        m_btnList->SetVisible(false);
        return;
    }
    else
    {
        m_infoVisible = kBasicInfo;
    }

    Clear();
    Update(im);

    m_btnList->SetVisible(true);
}


/*!
 \brief Remove infolist from display
 \return bool True if buttonlist was displayed/removed
*/
bool InfoList::Hide()
{
    // Only handle event if info currently displayed
    bool handled = (m_infoVisible != kNoInfo);
    m_infoVisible = kNoInfo;

    m_btnList->SetVisible(false);

    return handled;
}


/*!
 \brief Populate a buttonlist item with exif tag name & value
 \param name Exif tag name
 \param value Exif tag value
*/
void InfoList::CreateButton(const QString& name, const QString& value)
{
    if (value.isEmpty())
        return;

    auto *item = new MythUIButtonListItem(m_btnList, "");

    InfoMap infoMap;
    infoMap.insert("name", name);
    infoMap.insert("value", value);

    item->SetTextFromMap(infoMap);
}


/*!
 \brief Creates buttons detailing dir counts & size
 \param im Image dir
*/
void InfoList::CreateCount(ImageItemK &im)
{
    int dirCount = 0;
    int imageCount = 0;
    int videoCount = 0;
    int size = 0;
    m_mgr.GetDescendantCount(im.m_id, dirCount, imageCount, videoCount, size);

    QStringList report;
    if (imageCount > 0)
        report << tr("%Ln image(s)", "", imageCount);
    if (videoCount > 0)
        report << tr("%Ln video(s)", "", videoCount);
    if (dirCount > 0)
        report << tr("%Ln directory(ies)", "", dirCount);

    CreateButton(tr("Contains"), report.join(", "));
    CreateButton(tr("Dir size"), ImageAdapterBase::FormatSize(size));

    if (im.IsDevice() && im.IsLocal())
    {
        // Returns KiB
        auto fsInfo   = FileSystemInfo(QString(), im.m_filePath);
        int64_t total = fsInfo.getTotalSpace();
        int64_t used  = fsInfo.getUsedSpace();
        int64_t free  = fsInfo.getFreeSpace();
        if (total > 0)
        {
            CreateButton(tr("Free space"), tr("%L1 (%L2\%) Used: %L3 / %L4")
                         .arg(ImageAdapterBase::FormatSize(free))
                         .arg(100 * free / total)
                         .arg(ImageAdapterBase::FormatSize(used),
                              ImageAdapterBase::FormatSize(total)));
        }
    }
}


/*!
 \brief Populates available exif details for the current image/dir.
 \param im An image or dir
*/
void InfoList::Update(const ImagePtrK& im)
{
    if (!im || m_infoVisible == kNoInfo)
        return;

    if (im->m_id == GALLERY_DB_ID)
    {
        // No metadata for root
        Clear();
        CreateButton(tr("Name"), m_mgr.DeviceCaption(*im));
        CreateCount(*im);
        return;
    }

    // Request metadata
    m_mgr.RequestMetaData(im->m_id);
    // Reduce flicker by waiting for new data before clearing list
    m_timer.start();
}


/*!
 * \brief Build list of metadata tags
 * \param im Image
 * \param tagStrings Metadata encoded as list of key<sep>label<sep>value
 */
void InfoList::Display(ImageItemK &im, const QStringList &tagStrings)
{
    // Cancel timer & build list
    m_timer.stop();
    Clear();

    // Each tag has 3 elements: key, label, value
    ImageMetaData::TagMap tags = ImageMetaData::ToMap(tagStrings);

    QString tagHost = tags.take(EXIF_MYTH_HOST).value(2);
    QString tagPath = tags.take(EXIF_MYTH_PATH).value(2);
    QString tagName = tags.take(EXIF_MYTH_NAME).value(2);

    // Override SG
    if (im.m_id == PHOTO_DB_ID)
    {
        tagPath = tr("Storage Group");
        tagName = m_mgr.DeviceCaption(im);
    }

    CreateButton(tr("Name"), tagName);

    // Only show non-local hostnames
    QString host = (tagHost == gCoreContext->GetHostName()) ? "" : tagHost + ":";
    QString clone = (im.m_type == kCloneDir) ? tr("(and others)") : "";
    CreateButton(tr("Path"), QString("%1%2 %3").arg(host, tagPath, clone));

    if (im.IsDevice())
    {
        CreateButton(tr("Last scan"),
                     MythDate::toString(QDateTime::fromSecsSinceEpoch(im.m_date.count()),
                                        MythDate::kDateTimeFull | MythDate::kAddYear));
    }

    if (im.IsDirectory())
        CreateCount(im);

    if (!im.IsDevice())
    {
        CreateButton(tr("Modified"),
                     MythDate::toString(QDateTime::fromSecsSinceEpoch(im.m_modTime.count()),
                                        MythDate::kDateTimeFull | MythDate::kAddYear));
    }

    if (im.IsFile())
    {
        CreateButton(tr("File size"),   tags.take(EXIF_MYTH_SIZE).value(2));
        CreateButton(tr("Orientation"), tags.take(EXIF_MYTH_ORIENT).value(2));

        // Create buttons for exif/video tags
        // Multimap iterates each key latest->earliest so we must do it the long way
        QList groups = tags.uniqueKeys();
        for (const QString & group : std::as_const(groups))
        {
            // Iterate earliest->latest to preserve tag order
            using TagList = QList<QStringList>;
            TagList tagList = tags.values(group);
            TagList::const_iterator i = tagList.constEnd();

            if (m_infoVisible == kFullInfo)
            {
                // Show all tags
                while (i-- != tagList.constBegin())
                    CreateButton(i->at(1), i->at(2));
            }
            else
            {
                // Only show specific keys
                while (i-- != tagList.constBegin())
                    if (kBasicInfoSet.contains(i->at(0)))
                        CreateButton(i->at(1), i->at(2));
            }
        }

        // Only give list focus if requested
        if (m_btnList->CanTakeFocus())
            m_screen.SetFocusWidget(m_btnList);
    }
}

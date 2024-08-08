// Qt
#include <QStringBuilder>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"

// MythFrontend
#include "galleryconfig.h"
#include "gallerytransitions.h"

#define TR GallerySettings::tr

StandardSetting *GallerySettings::ImageOrder() const
{
    auto *gc = new HostComboBoxSetting("GalleryImageOrder");

    gc->setLabel(TR("Image Order"));
    gc->setHelpText(TR("The order that pictures/videos are shown in thumbnail "
                       "view and ordered slideshows."));

    gc->addSelection(TR("Filename (A-Z)"),
                     QString::number(kSortByNameAsc));
    gc->addSelection(TR("Reverse Filename (Z-A)"),
                     QString::number(kSortByNameDesc));
    gc->addSelection(TR("Exif Date (oldest first)"),
                     QString::number(kSortByDateAsc));
    gc->addSelection(TR("Reverse Exif Date (newest first)"),
                     QString::number(kSortByDateDesc));
    gc->addSelection(TR("File Modified Time (oldest first)"),
                     QString::number(kSortByModTimeAsc));
    gc->addSelection(TR("Reverse File Modified Time (newest first)"),
                     QString::number(kSortByModTimeDesc));
    gc->addSelection(TR("File Extension (A-Z)"),
                     QString::number(kSortByExtAsc));
    gc->addSelection(TR("Reverse File Extension (Z-A)"),
                     QString::number(kSortByExtDesc));
    gc->addSelection(TR("File Size (smallest first)"),
                     QString::number(kSortBySizeAsc));
    gc->addSelection(TR("Reverse File Size (largest first)"),
                     QString::number(kSortBySizeDesc));

    connect(gc,   &StandardSetting::ChangeSaved,
            this, &GallerySettings::OrderChanged);

    return gc;
}

StandardSetting *GallerySettings::DirOrder() const
{
    auto *gc = new HostComboBoxSetting("GalleryDirOrder");

    gc->setLabel(TR("Directory Order"));
    gc->setHelpText(TR("The order that dirctories are shown and traversed "
                       "in recursive slideshows."));

    gc->addSelection(TR("Filename (A-Z)"), QString::number(kSortByNameAsc));
    gc->addSelection(TR("Reverse Filename (Z-A)"), QString::number(kSortByNameDesc));
    gc->addSelection(TR("File Modified Time (oldest first)"), QString::number(kSortByModTimeAsc));
    gc->addSelection(TR("Reverse File Modified Time (newest first)"), QString::number(kSortByModTimeDesc));

    connect(gc,   &StandardSetting::ChangeSaved,
            this, &GallerySettings::OrderChanged);

    return gc;
}

static void AddFormat(HostComboBoxSetting* gc, const QDateTime& date, const QString& format)
{ gc->addSelection(gCoreContext->GetQLocale().toString(date, format), format); }

StandardSetting *GallerySettings::DateFormat() const
{
    auto *gc = new HostComboBoxSetting("GalleryDateFormat");

    gc->setLabel(TR("Date Format"));
    gc->setHelpText(TR("Date format of thumbnail captions. Other places use the system date format. "
                       "Sample shows 3rd May 2002."));

    QDateTime sampdate = MythDate::fromString("2002-05-03");

    AddFormat(gc, sampdate, "dd/MM/yy");
    AddFormat(gc, sampdate, "dd-MM-yy");
    AddFormat(gc, sampdate, "dd.MM.yy");
    AddFormat(gc, sampdate, "d/M/yy");
    AddFormat(gc, sampdate, "d-M-yy");
    AddFormat(gc, sampdate, "d.M.yy");
    AddFormat(gc, sampdate, "MM/dd/yy");
    AddFormat(gc, sampdate, "MM-dd-yy");
    AddFormat(gc, sampdate, "MM.dd.yy");
    AddFormat(gc, sampdate, "M/d/yy");
    AddFormat(gc, sampdate, "M-d-yy");
    AddFormat(gc, sampdate, "M.d.yy");
    AddFormat(gc, sampdate, "yyyy/MM/dd");
    AddFormat(gc, sampdate, "yyyy-MM-dd");
    AddFormat(gc, sampdate, "yyyy.MM.dd");
    AddFormat(gc, sampdate, QString("yyyy") % QChar(0x5E74) %
              "M" % QChar(0x6708) % "d" % QChar(0x65E5)); // yyyy年M月d日

    connect(gc,   &StandardSetting::ChangeSaved,
            this, &GallerySettings::DateChanged);

    return gc;
}

static StandardSetting *TransitionType()
{
    auto *gc = new HostComboBoxSetting("GalleryTransitionType");

    gc->setLabel(TR("Transition"));
    gc->setHelpText(TR("Effect to use between slides"));

    // Initialise selected transition
    TransitionRegistry availableTransitions(GetMythPainter()->SupportsAnimation());
    TransitionMap transitions = availableTransitions.GetAll();
    QMapIterator<int, Transition*> i(transitions);
    while (i.hasNext())
    {
        i.next();
        gc->addSelection(i.value()->objectName(), QString::number(i.key()));
    }

    return gc;
}

static StandardSetting *SlideDuration()
{
    auto *gc = new HostSpinBoxSetting("GallerySlideShowTime", 100, 60000, 100, 5);

    gc->setLabel(TR("Slide Duration (ms)"));
    gc->setHelpText(TR("The time that a slide is displayed (between transitions), "
                       "in milliseconds."));
    return gc;
}

static StandardSetting *TransitionDuration()
{
    auto *gc = new HostSpinBoxSetting("GalleryTransitionTime", 100, 60000, 100, 5);

    gc->setLabel(TR("Transition Duration (ms)"));
    gc->setHelpText(TR("The time that each transition lasts, in milliseconds."));
    return gc;
}

static StandardSetting *StatusDelay()
{
    auto *gc = new HostSpinBoxSetting("GalleryStatusDelay", 0, 10000, 50, 10);

    gc->setLabel(TR("Status Delay (ms)"));
    gc->setHelpText(TR("The delay before showing the Loading/Playing status, "
                       "in milliseconds."));
    return gc;
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
StandardSetting *GallerySettings::ImageMaximumSize() const
{
    auto *gc = new HostSpinBoxSetting("ImageMaximumSize", -1, 1024*1024, 1, 10);

    gc->setLabel(TR("Maximum Image Size (MB)"));
    gc->setHelpText(TR("The maximum image size that will be loaded, "
                       "in megabytes. (-1 means system default, 0 "
                       "means unlimited.)"));

    connect(gc,   &StandardSetting::ChangeSaved,
            this, &GallerySettings::ImageSizeChanged);

    return gc;
}

void GallerySettings::ImageSizeChanged ()
{
    int maxImageSize = gCoreContext->GetNumSetting("ImageMaximumSize", -1);
    if (maxImageSize < 0)
        maxImageSize = 128; // Restore Qt6 default
    QImageReader::setAllocationLimit(maxImageSize);
}
#endif

static StandardSetting *UseTransitions()
{
    auto *gc = new HostCheckBoxSetting("GalleryBrowseTransition");

    gc->setLabel(TR("Use transitions when browsing"));
    gc->setHelpText(TR("When cleared, transitions will only be used "
                       "during a slideshow."));
    return gc;
}

/*!
 \brief Setting for Importing via script
 \param enabled True if password has been entered
*/
static StandardSetting *Import(bool enabled)
{
    auto *gc = new HostTextEditSetting("GalleryImportCmd");

    gc->setVisible(enabled);
    gc->setLabel(TR("Import Command"));
    gc->setHelpText(TR("Command/script that can be run from the menu. "
                       "\n%TMPDIR% will be replaced by a new temporary directory, "
                       "which the import dialog will show automatically. The "
                       "directory will be removed when Gallery exits."));
    return gc;
}

/*!
 \brief Setting for excluding image files by pattern
 \param enabled True if password has been entered
*/
StandardSetting *GallerySettings::Exclusions(bool enabled) const
{
    auto *gc = new GlobalTextEditSetting("GalleryIgnoreFilter");

    gc->setVisible(enabled);
    gc->setLabel(TR("Scanner Exclusions"));
    gc->setHelpText(TR("Comma-separated list of filenames/directory names "
                       "to be ignored when scanning. "
                       "Glob wildcards * and ? are valid."));

    connect(gc,   &StandardSetting::ChangeSaved,
            this, &GallerySettings::ExclusionsChanged);

    return gc;
}

/*!
 \brief Setting for running gallery on start-up
 \param enabled True if password has been entered
*/
static StandardSetting *Autorun(bool enabled)
{
    auto *gc = new HostCheckBoxSetting("GalleryAutoStart");

    gc->setVisible(enabled);
    gc->setLabel(TR("Start Gallery when media inserted"));
    gc->setHelpText(TR("Set to automatically start Gallery when media "
                       "(USB/CD's containing images) are inserted."));
    return gc;
}

/*!
 \brief Setting for changing password
 \param enabled True if password has been entered
*/
static StandardSetting *Password(bool enabled)
{
    auto *gc = new GlobalTextEditSetting("GalleryPassword");

    gc->setVisible(enabled);
    gc->setLabel(TR("Password"));
    gc->setHelpText(TR("When set all actions that modify the filesystem or "
                       "database are protected (copy, move, transform, "
                       "hiding, covers). Hidden items cannot be viewed. "
                       "Applies to all frontends. "
                       "\nDisabled by an empty password. "
                       "Privileges persist until Gallery exits to main menu."));
    return gc;
}

/*!
 \brief Setting for clearing image database
 \param enabled True if password has been entered
*/
StandardSetting *GallerySettings::ClearDb(bool enabled) const
{
    auto *gc = new ButtonStandardSetting(TR("Reset Image Database"));

    gc->setVisible(enabled);
    gc->setLabel(TR("Reset Image Database"));
    gc->setHelpText(TR("Clears the database and thumbnails for the Image Storage Group. "
                       "A rescan will be required. Images for local media will persist."));

    connect(gc,    &ButtonStandardSetting::clicked,
            this,  &GallerySettings::ShowConfirmDialog);

    return gc;
}

void GallerySettings::ShowConfirmDialog()
{
    QString msg(TR("Warning! This will erase settings for: hidden images, "
                   "directory covers and re-orientations. "
                   "You will have to set them again after re-scanning."));
    auto *stack = GetMythMainWindow()->GetStack("popup stack");
    auto *dialog = new MythConfirmationDialog(stack, msg);
    if (dialog->Create())
    {
        stack->AddScreen(dialog);
        connect(dialog, &MythConfirmationDialog::haveResult,
                this,   [this](bool ok) { if (ok) emit ClearDbPressed(); });
    }
    else
    {
        delete dialog;
    }
}

GallerySettings::GallerySettings(bool enable)
{
    setLabel(TR("Gallery Settings"));

    addChild(ImageOrder());
    addChild(DirOrder());
    addChild(DateFormat());
    addChild(TransitionType());
    addChild(SlideDuration());
    addChild(TransitionDuration());
    addChild(StatusDelay());
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    addChild(ImageMaximumSize());
#endif
    addChild(UseTransitions());

    // These modify the database
    addChild(Import(enable));
    addChild(Exclusions(enable));
    addChild(Autorun(enable));
    addChild(Password(enable));
    addChild(ClearDb(enable));
}

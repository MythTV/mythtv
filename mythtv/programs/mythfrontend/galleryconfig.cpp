#include "galleryconfig.h"

#include <QStringBuilder>

#include "mythcorecontext.h"
#include "mythdate.h"
#include "gallerytransitions.h"

#define ADD_FORMAT(date, format) fmt->addSelection(gCoreContext->GetQLocale().toString(date, format), format)


ThumbSettings::ThumbSettings() : VerticalConfigurationGroup()
{
    setLabel(tr("Thumbnails"));

    HostComboBox *order = new HostComboBox("GalleryImageOrder");
    order->setLabel(tr("Image Order"));
    order->setHelpText(tr("The order that pictures/videos are shown in thumbnail "
                          "view and ordered slideshows."));
    order->addSelection(tr("Filename (A-Z)"), QString::number(kSortByNameAsc));
    order->addSelection(tr("Reverse Filename (Z-A)"), QString::number(kSortByNameDesc));
    order->addSelection(tr("Exif Date (oldest first)"), QString::number(kSortByDateAsc));
    order->addSelection(tr("Reverse Exif Date (newest first)"), QString::number(kSortByDateDesc));
    order->addSelection(tr("File Modified Time (oldest first)"), QString::number(kSortByModTimeAsc));
    order->addSelection(tr("Reverse File Modified Time (newest first)"), QString::number(kSortByModTimeDesc));
    order->addSelection(tr("File Extension (A-Z)"), QString::number(kSortByExtAsc));
    order->addSelection(tr("Reverse File Extension (Z-A)"), QString::number(kSortByExtDesc));
    order->addSelection(tr("File Size (smallest first)"), QString::number(kSortBySizeAsc));
    order->addSelection(tr("Reverse File Size (largest first)"), QString::number(kSortBySizeDesc));
    addChild(order);

    HostComboBox *orderDir = new HostComboBox("GalleryDirOrder");
    orderDir->setLabel(tr("Directory Order"));
    orderDir->setHelpText(tr("The order that dirctories are shown and traversed "
                             "in recursive slideshows."));
    orderDir->addSelection(tr("Filename (A-Z)"), QString::number(kSortByNameAsc));
    orderDir->addSelection(tr("Reverse Filename (Z-A)"), QString::number(kSortByNameDesc));
    orderDir->addSelection(tr("File Modified Time (oldest first)"), QString::number(kSortByModTimeAsc));
    orderDir->addSelection(tr("Reverse File Modified Time (newest first)"), QString::number(kSortByModTimeDesc));
    addChild(orderDir);

    HostComboBox *fmt = new HostComboBox("GalleryDateFormat");
    fmt->setLabel(tr("Date Format"));

    QDateTime sampdate = MythDate::fromString("2002-05-03");

    ADD_FORMAT(sampdate, "dd/MM/yy");
    ADD_FORMAT(sampdate, "dd-MM-yy");
    ADD_FORMAT(sampdate, "d/M/yy");
    ADD_FORMAT(sampdate, "d-M-yy");
    ADD_FORMAT(sampdate, "MM/dd/yy");
    ADD_FORMAT(sampdate, "MM-dd-yy");
    ADD_FORMAT(sampdate, "M/d/yy");
    ADD_FORMAT(sampdate, "M-d-yy");
    ADD_FORMAT(sampdate, "yyyy/MM/dd");
    ADD_FORMAT(sampdate, "yyyy-MM-dd");
    ADD_FORMAT(sampdate, QString("yyyy") % QChar(0x5E74) %
               "M" % QChar(0x6708) % "d" % QChar(0x65E5)); // yyyy年M月d日

    fmt->setHelpText(tr("Date format of thumbnail captions. Other places use the system date format. "
                        "Sample shows 3rd May 2002."));
    addChild(fmt);
}


SlideSettings::SlideSettings() : VerticalConfigurationGroup()
{
    setLabel(tr("Slideshow"));

    HostComboBox *tranBox = new HostComboBox("GalleryTransitionType");
    tranBox->setLabel(tr("Transition"));
    tranBox->setHelpText(tr("Effect to use between slides"));

    // Initialise selected transition
    TransitionRegistry availableTransitions(GetMythPainter()->SupportsAnimation());
    TransitionMap transitions = availableTransitions.GetAll();
    QMapIterator<int, Transition*> i(transitions);
    while (i.hasNext())
    {
        i.next();
        tranBox->addSelection(i.value()->objectName(), QString::number(i.key()));
    }
    addChild(tranBox);

    HostSpinBox *slide = new HostSpinBox("GallerySlideShowTime", 100, 60000, 100);
    slide->setLabel(tr("Slide Duration (ms)"));
    slide->setHelpText(tr("The time that a slide is displayed (between transitions), "
                          "in milliseconds."));
    addChild(slide);

    HostSpinBox *transition = new HostSpinBox("GalleryTransitionTime", 100, 60000, 100);
    transition->setLabel(tr("Transition Duration (ms)"));
    transition->setHelpText(tr("The time that each transition lasts, in milliseconds."));
    addChild(transition);

    HostCheckBox *browseTran = new HostCheckBox("GalleryBrowseTransition");
    browseTran->setLabel(tr("Use transitions when browsing"));
    browseTran->setHelpText(tr("When cleared, transitions will only be used "
                               "during a slideshow."));
    addChild(browseTran);
}


/*!
 \brief Settings Page 1
*/
GallerySettings::GallerySettings() : VerticalConfigurationGroup(false)
{
    setLabel(tr("Gallery Settings"));

    addChild(new ThumbSettings());
    addChild(new SlideSettings());
}


/*!
 \brief Settings for Importing
 \param enable True if password has been entered
*/
ImportSettings::ImportSettings(bool enable) : VerticalConfigurationGroup()
{
    setLabel(tr("Import"));
    setEnabled(enable);

    HostLineEdit *script = new HostLineEdit("GalleryImportCmd", true);
    script->setLabel(tr("Import Command"));
    script->setHelpText(tr("Command/script that can be run from the menu. "
                           "\n%TMPDIR% will be replaced by a new temporary directory, "
                           "which the import dialog will show automatically. The "
                           "directory will be removed when Gallery exits."));

    script->setEnabled(enable);
    addChild(script);
}


/*!
 \brief Settings Page 2
 \param enable True if password has been entered
*/
DatabaseSettings::DatabaseSettings(bool enable)
    : VerticalConfigurationGroup(false)
{
    setLabel(tr("Database Settings") + (enable ? "" : tr(" (Requires edit privileges)")));

    addChild(new ImportSettings(enable));

    // Exclusions - Use stacked to preserve spacing
    StackedConfigurationGroup *group1 = new StackedConfigurationGroup(false, false);
    addChild(group1);

    GlobalLineEdit *exclusions = new GlobalLineEdit("GalleryIgnoreFilter");
    exclusions->setLabel(tr("Scanner Exclusions"));
    exclusions->setHelpText(tr("Comma-separated list of filenames/directory names "
                               "to be ignored when scanning. "
                               "Glob wildcards * and ? are valid."));
    exclusions->setEnabled(enable);
    group1->addChild(exclusions);

    // Autorun - Use stacked to preserve spacing
    StackedConfigurationGroup *group4 = new StackedConfigurationGroup(false, false);
    addChild(group4);

    HostCheckBox *autorun = new HostCheckBox("GalleryAutoStart");
    autorun->setLabel(tr("Start Gallery when media inserted"));
    autorun->setHelpText(tr("Set to automatically start Gallery when media "
                            "(USB/CD's containing images) are inserted."));
    autorun->setEnabled(enable);
    group4->addChild(autorun);

    // Password - Use stacked to preserve spacing
    StackedConfigurationGroup *group2 = new StackedConfigurationGroup(false, false);
    addChild(group2);

    GlobalLineEdit *password = new GlobalLineEdit("GalleryPassword");
    password->setLabel(tr("Password"));
    password->SetPasswordEcho(true);
    password->setHelpText(tr("When set all actions that modify the filesystem or "
                             "database are protected (copy, move, transform, "
                             "hiding, covers). Hidden items cannot be viewed. "
                             "Applies to all frontends. "
                             "\nDisabled by an empty password. "
                             "Privileges persist until Gallery exits to main menu."));
    password->setEnabled(enable);
    group2->addChild(password);

    // Clear Db
    TriggeredConfigurationGroup *group3 = new TriggeredConfigurationGroup(false, false);
    group3->SetVertical(false);
    addChild(group3);

    TransCheckBoxSetting *clear = new TransCheckBoxSetting();
    clear->setLabel(tr("Reset Image Database"));
    clear->setHelpText(tr("Clears the database and thumbnails for the Image Storage Group. "
                          "A rescan will be required. Images for local media will persist."));
    clear->setEnabled(enable);
    group3->addChild(clear);

    HorizontalConfigurationGroup *clrSub =new HorizontalConfigurationGroup(false, false);

    TransButtonSetting *confirm = new TransButtonSetting("clearDb");
    confirm->setLabel(tr("Clear Now!"));
    confirm->setHelpText(tr("Warning! This will erase settings for: hidden images, "
                            "directory covers and re-orientations. "
                            "You will have to set them again after re-scanning."));
    connect(confirm, SIGNAL(pressed()), this, SIGNAL(ClearDbPressed()));
    clrSub->addChild(confirm);

    group3->setTrigger(clear);
    group3->addTarget("0", new HorizontalConfigurationGroup(false, false));
    group3->addTarget("1", clrSub);
}

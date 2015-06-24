#include "galleryconfig.h"

#include "gallerytransitions.h"


/*!
 \brief Settings for Thumbnail view
*/
class ThumbSettings : public VerticalConfigurationGroup
{
public:
    ThumbSettings() : VerticalConfigurationGroup()
    {
        setLabel(tr("Thumbnails"));

        HostComboBox *order = new HostComboBox("GallerySortOrder");
        order->setLabel(tr("Image Order"));
        order->setHelpText(tr("The order that images are shown in thumbnail view "
                              "and (ordered) slideshows."));
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
    }
};


/*!
 \brief Settings for Slideshow view
*/
class SlideSettings : public VerticalConfigurationGroup
{
public:
    SlideSettings() : VerticalConfigurationGroup()
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
            tranBox->addSelection(i.value()->GetName(), QString::number(i.key()));
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
    }
};


/*!
 \brief Settings Page 1
*/
GallerySettings::GallerySettings() : VerticalConfigurationGroup(false)
{
    setLabel(tr("Gallery Settings"));

    addChild(new ThumbSettings());
    addChild(new SlideSettings());
}


class ImportSettings : public VerticalConfigurationGroup
{
public:
    /*!
     \brief Settings for Importing
     \param enable True if password has been entered
    */
    ImportSettings(bool enable) : VerticalConfigurationGroup()
    {
        setLabel(tr("Import"));
        setEnabled(enable);

        HostLineEdit *import = new HostLineEdit("GalleryImportLocation", true);
        import->setLabel(tr("Import Path"));
        import->setHelpText(tr("The path where the Import dialog usually starts."));
        import->setEnabled(enable);
        addChild(import);

        TriggeredConfigurationGroup *group = new TriggeredConfigurationGroup(false, false);
        group->SetVertical(false);
        addChild(group);

        HostCheckBox *useScript = new HostCheckBox("GalleryUseImportCmd");
        useScript->setLabel(tr("Use Import Command"));
        useScript->setHelpText(tr("Defines a command/script to aid importing. "
                                  "Useful if a camera doesn't provide a mountable filesystem "
                                  "and you need an alternative way of transferring images."));
        useScript->setEnabled(enable);
        group->addChild(useScript);

        HostLineEdit *script = new HostLineEdit("GalleryImportCmd", true);
        script->setLabel(tr(""));
        script->setHelpText(tr("Command/script that can be run from the menu. "
                               "%DIR% will be replaced by the Import Path."
                               "\n%TMPDIR% will be replaced by a new temporary directory, "
                               "which the import dialog will show automatically. The "
                               "directory will be removed when Gallery exits."));

        script->setEnabled(enable);
        group->setTrigger(useScript);
        group->addTarget("0", new HorizontalConfigurationGroup(false, false));
        group->addTarget("1", script);
    }
};



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

    // Password - Use stacked to preserve spacing
    StackedConfigurationGroup *group2 = new StackedConfigurationGroup(false, false);
    addChild(group2);

    HostLineEdit *password = new HostLineEdit("GalleryPassword");
    password->setLabel(tr("Password"));
    password->SetPasswordEcho(true);
    password->setHelpText(tr("If set then all actions that modify the filesystem or "
                             "database are password-protected (copy, move, import, "
                             "transform, hiding, set covers). Protection is disabled "
                             "by an empty password.\nPrivileges persist until "
                             "Gallery exits to main menu."));
    password->setEnabled(enable);
    group2->addChild(password);

    // Clear Db
    TriggeredConfigurationGroup *group3 = new TriggeredConfigurationGroup(false, false);
    group3->SetVertical(false);
    addChild(group3);

    TransCheckBoxSetting *clear = new TransCheckBoxSetting();
    clear->setLabel(tr("Reset Image Database"));
    clear->setHelpText(tr("Clears the Image Database and all thumbnails. A rescan "
                          "will be required."));
    clear->setEnabled(enable);
    group3->addChild(clear);

    HorizontalConfigurationGroup *clrSub =new HorizontalConfigurationGroup(false, false);

    TransButtonSetting *confirm = new TransButtonSetting("clearDb");
    confirm->setLabel(tr("Clear Now!"));
    confirm->setHelpText(tr("Warning! This will erase settings for: hidden images, "
                            "directory covers and modified orientations. "
                            "You will have to set them again after re-scanning."));
    connect(confirm, SIGNAL(pressed()), this, SLOT(ClearDb()));
    clrSub->addChild(confirm);

    group3->setTrigger(clear);
    group3->addTarget("0", new HorizontalConfigurationGroup(false, false));
    group3->addTarget("1", clrSub);
}

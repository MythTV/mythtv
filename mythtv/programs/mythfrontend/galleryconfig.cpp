// Qt headers

// MythTV headers
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythuitextedit.h"
#include "mythuicheckbox.h"
#include "mythuibutton.h"
#include "mythuibuttonlist.h"
#include "mythuispinbox.h"

#include "galleryconfig.h"
#include "gallerytypedefs.h"



enum FileSortOrder {
    kSortByNameAsc     = 0,
    kSortByNameDesc    = 1,
    kSortByModTimeAsc  = 2,
    kSortByModTimeDesc = 3,
    kSortByExtAsc      = 4,
    kSortByExtDesc     = 5,
    kSortBySizeAsc     = 6,
    kSortBySizeDesc    = 7,
    kSortByDateAsc     = 8,
    kSortByDateDesc    = 9
};



/** \fn     GalleryConfig::GalleryConfig(MythScreenStack *, const char *)
 *  \brief  Constructor
 *  \param  parent The screen parent
 *  \param  name The name of the screen
 *  \return void
 */
GalleryConfig::GalleryConfig(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name),
      m_storageGroupName(NULL),
      m_sortOrder(NULL),
      m_slideShowTime(NULL),
      m_transitionType(NULL),
      m_transitionTime(NULL),
      m_showHiddenFiles(NULL),
      m_saveButton(NULL),
      m_cancelButton(NULL)
{
    // preset or load all variables
    m_sortOrder = 0;
}



/** \fn     GalleryConfig::~GalleryConfig()
 *  \brief  Destructor
 *  \return void
 */
GalleryConfig::~GalleryConfig()
{

}



/** \fn     GalleryConfig::Create()
 *  \brief  Initialises and shows the graphical elements
 *  \return void
 */
bool GalleryConfig::Create()
{
    // Load the theme for this screen
    if (!LoadWindowFromXML("image-ui.xml", "galleryconfig", this))
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_storageGroupName, "storagegroupname", &err);
    UIUtilE::Assign(this, m_sortOrder, "sortorder", &err);
    UIUtilE::Assign(this, m_slideShowTime, "slideshowtime", &err);
    UIUtilE::Assign(this, m_transitionType, "transitiontype", &err);
    UIUtilE::Assign(this, m_transitionTime, "transitiontime", &err);
    UIUtilE::Assign(this, m_showHiddenFiles, "showhiddenfiles", &err);

    UIUtilE::Assign(this, m_saveButton, "save", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel", &err);

    // check if all widgets are present
    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    // Load the values from the database
    Load();

    // connect the widgets to their methods
    connect(m_saveButton, SIGNAL(Clicked()), this, SLOT(Save()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Exit()));

    BuildFocusList();

    SetFocusWidget(m_storageGroupName);

    return true;
}



/** \fn     GalleryConfig::keyPressEvent(QKeyEvent *)
 *  \brief  Translates the keypresses and keys bound to the
 *          plugin to specific actions within the plugin
 *  \param  event The pressed key
 *  \return bool True if the key was used, otherwise false
 */
bool GalleryConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}



/** \fn     GalleryConfig::Load()
 *  \brief  Load the values from the database and adds them to the widgets
 *  \return void
 */
void GalleryConfig::Load()
{
    m_storageGroupName->SetText(gCoreContext->GetSetting("GalleryStorageGroupName", "Pictures"));

    new MythUIButtonListItem(m_sortOrder, tr("Name (A-Z alpha)"),
                             qVariantFromValue(QString::number(kSortByNameAsc)));
    new MythUIButtonListItem(m_sortOrder, tr("Reverse Name (Z-A alpha)"),
                             qVariantFromValue(QString::number(kSortByNameDesc)));
    new MythUIButtonListItem(m_sortOrder, tr("Mod Time (oldest first)"),
                             qVariantFromValue(QString::number(kSortByModTimeAsc)));
    new MythUIButtonListItem(m_sortOrder, tr("Reverse Mod Time (newest first)"),
                             qVariantFromValue(QString::number(kSortByModTimeDesc)));
    new MythUIButtonListItem(m_sortOrder, tr("Extension (A-Z alpha)"),
                             qVariantFromValue(QString::number(kSortByExtAsc)));
    new MythUIButtonListItem(m_sortOrder, tr("Reverse Extension (Z-A alpha)"),
                             qVariantFromValue(QString::number(kSortByExtDesc)));
    new MythUIButtonListItem(m_sortOrder, tr("Filesize (smallest first)"),
                             qVariantFromValue(QString::number(kSortBySizeAsc)));
    new MythUIButtonListItem(m_sortOrder, tr("Reverse Filesize (largest first)"),
                             qVariantFromValue(QString::number(kSortBySizeDesc)));
    new MythUIButtonListItem(m_sortOrder, tr("Date (oldest first)"),
                             qVariantFromValue(QString::number(kSortByDateAsc)));
    new MythUIButtonListItem(m_sortOrder, tr("Reverse Date (neweset first)"),
                             qVariantFromValue(QString::number(kSortByDateDesc)));
    m_sortOrder->SetValueByData(gCoreContext->GetNumSetting("GallerySortOrder", kSortByDateAsc));

    m_slideShowTime->SetRange(0, 30000, 500);
    m_slideShowTime->SetValue(gCoreContext->GetSetting("GallerySlideShowTime", "3000"));

    new MythUIButtonListItem(m_transitionType, tr("None"), qVariantFromValue(0));
    new MythUIButtonListItem(m_transitionType, tr("Fade"), qVariantFromValue(1));
    m_transitionType->SetValueByData(gCoreContext->GetNumSetting("GalleryTransitionType", kFade));

    m_transitionTime->SetRange(0, 5000, 100);
    m_transitionTime->SetValue(gCoreContext->GetSetting("GalleryTransitionTime", "1000"));

    int setting = gCoreContext->GetNumSetting("GalleryShowHiddenFiles", 0);
    if (setting == 1)
        m_showHiddenFiles->SetCheckState(MythUIStateType::Full);
}



/** \fn     GalleryConfig::Save()
 *  \brief  Saves the values from the widgets into the database
 *  \return void
 */
void GalleryConfig::Save()
{
    gCoreContext->SaveSetting("GalleryStorageGroupName",
                              m_storageGroupName->GetText());
    gCoreContext->SaveSetting("GallerySortOrder",
                              m_sortOrder->GetDataValue().toString());
    gCoreContext->SaveSetting("GallerySlideShowTime",
                              m_slideShowTime->GetValue());
    gCoreContext->SaveSetting("GalleryTransitionType",
                              m_transitionType->GetDataValue().toString());
    gCoreContext->SaveSetting("GalleryTransitionTime",
                              m_transitionTime->GetValue());

    int checkstate = (m_showHiddenFiles->GetCheckState() == MythUIStateType::Full) ? 1 : 0;
    gCoreContext->SaveSetting("GalleryShowHiddenFiles", checkstate);

    // tell the main view to reload the images
    // because the storage group dir might have changed
    emit configSaved();

    Close();
}



/** \fn     GalleryConfig::Exit()
 *  \brief  Exits the configuration screen
 *  \return void
 */
void GalleryConfig::Exit()
{
    Close();
}

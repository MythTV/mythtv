// C++
#include <iostream>
using namespace std;

// Qt
#include <QString>

// MythTV
#include <mythmainwindow.h>
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdirs.h>
#include <mythuicheckbox.h>
#include <mythuibuttonlist.h>
#include <mythsystemlegacy.h>

// mythbrowser
#include "bookmarkmanager.h"
#include "bookmarkeditor.h"
#include "browserdbutil.h"
#include "mythbrowser.h"
#include "mythflashplayer.h"

// ---------------------------------------------------

BrowserConfig::BrowserConfig(MythScreenStack *parent, const char *name) :
    MythScreenType(parent, name),
    m_commandEdit(NULL),     m_zoomEdit(NULL),
    m_descriptionText(NULL), m_titleText(NULL),
    m_enablePluginsCheck(NULL),
    m_okButton(NULL),        m_cancelButton(NULL)
{
}

bool BrowserConfig::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "browserconfig", this);

    if (!foundtheme)
        return false;

    m_titleText = dynamic_cast<MythUIText *> (GetChild("title"));

    if (m_titleText)
          m_titleText->SetText(tr("MythBrowser Settings"));

    m_commandEdit = dynamic_cast<MythUITextEdit *> (GetChild("command"));
    m_zoomEdit = dynamic_cast<MythUITextEdit *> (GetChild("zoom"));
    m_enablePluginsCheck = dynamic_cast<MythUICheckBox *> (GetChild("enablepluginscheck"));

    m_descriptionText = dynamic_cast<MythUIText *> (GetChild("description"));

    m_okButton = dynamic_cast<MythUIButton *> (GetChild("ok"));
    m_cancelButton = dynamic_cast<MythUIButton *> (GetChild("cancel"));

    if (!m_commandEdit || !m_zoomEdit || !m_enablePluginsCheck || !m_okButton || !m_cancelButton)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    m_commandEdit->SetText(gCoreContext->GetSetting("WebBrowserCommand",
                           "Internal"));

    m_zoomEdit->SetText(gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0"));

    int setting = gCoreContext->GetNumSetting("WebBrowserEnablePlugins", 1);
    if (setting == 1)
        m_enablePluginsCheck->SetCheckState(MythUIStateType::Full);

    connect(m_okButton, SIGNAL(Clicked()), this, SLOT(slotSave()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_commandEdit,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_zoomEdit   ,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_enablePluginsCheck,  SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_okButton,     SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));
    connect(m_cancelButton, SIGNAL(TakingFocus()), SLOT(slotFocusChanged()));

    BuildFocusList();

    SetFocusWidget(m_commandEdit);

    return true;
}

BrowserConfig::~BrowserConfig()
{
}

void BrowserConfig::slotSave(void)
{
    float zoom = m_zoomEdit->GetText().toFloat();
    if (zoom > 5.0)
        zoom = 5.0;
    if (zoom < 0.3)
        zoom = 0.3; 
    gCoreContext->SaveSetting("WebBrowserZoomLevel", QString("%1").arg(zoom));
    gCoreContext->SaveSetting("WebBrowserCommand", m_commandEdit->GetText());
    int checkstate = 0;
    if (m_enablePluginsCheck->GetCheckState() == MythUIStateType::Full)
        checkstate = 1;
    gCoreContext->SaveSetting("WebBrowserEnablePlugins", checkstate);

    Close();
}

bool BrowserConfig::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BrowserConfig::slotFocusChanged(void)
{
    if (!m_descriptionText)
        return;

    QString msg = "";
    if (GetFocusWidget() == m_commandEdit)
        msg = tr("This is the command that will be used to show the web browser. "
                 "Use 'Internal' to use the built in web browser'. "
                 "%ZOOM% and %URL% will be replaced with the zoom level and URL list.");
    else if (GetFocusWidget() == m_zoomEdit)
        msg = tr("This is the default text size that will be used. Valid values "
                 "for the Internal browser are from 0.3 to 5.0 with 1.0 being "
                 "normal size less than 1 is smaller and greater than 1 is "
                 "larger than normal size.");
    else if (GetFocusWidget() == m_enablePluginsCheck)
        msg = tr("If checked this will enable browser plugins if the 'Internal' "
                 "browser is being used.");
    else if (GetFocusWidget() == m_cancelButton)
        msg = tr("Exit without saving settings");
    else if (GetFocusWidget() == m_okButton)
        msg = tr("Save settings and Exit");

    m_descriptionText->SetText(msg);
}

// ---------------------------------------------------

BookmarkManager::BookmarkManager(MythScreenStack *parent, const char *name)
               : MythScreenType(parent, name)
{
    m_bookmarkList = NULL;
    m_groupList = NULL;
    m_messageText = NULL;
    m_menuPopup = NULL;
}

bool BookmarkManager::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("browser-ui.xml", "bookmarkmanager", this);

    if (!foundtheme)
        return false;

    m_groupList = dynamic_cast<MythUIButtonList *>(GetChild("grouplist"));
    m_bookmarkList = dynamic_cast<MythUIButtonList *>(GetChild("bookmarklist"));

    // optional text area warning user hasn't set any bookmarks yet
    m_messageText = dynamic_cast<MythUIText *>(GetChild("messagetext"));
    if (m_messageText)
        m_messageText->SetText(tr("No bookmarks defined.\n\n"
                "Use the 'Add Bookmark' menu option to add new bookmarks"));

    if (!m_groupList || !m_bookmarkList)
    {
        LOG(VB_GENERAL, LOG_ERR, "Theme is missing critical theme elements.");
        return false;
    }

    GetSiteList(m_siteList);
    UpdateGroupList();
    UpdateURLList();

    connect(m_groupList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(slotGroupSelected(MythUIButtonListItem*)));

    connect(m_bookmarkList, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(slotBookmarkClicked(MythUIButtonListItem*)));

    BuildFocusList();

    SetFocusWidget(m_groupList);

    return true;
}

BookmarkManager::~BookmarkManager()
{
    while (!m_siteList.isEmpty())
        delete m_siteList.takeFirst();
}

void BookmarkManager::UpdateGroupList(void)
{
    m_groupList->Reset();
    QStringList groups;
    for (int x = 0; x < m_siteList.count(); x++)
    {
        Bookmark *site = m_siteList.at(x);

        if (groups.indexOf(site->category) == -1)
        {
            groups.append(site->category);
            new MythUIButtonListItem(m_groupList, site->category);
        }
    }
}

void BookmarkManager::UpdateURLList(void)
{
    m_bookmarkList->Reset();

    if (m_messageText)
        m_messageText->SetVisible((m_siteList.count() == 0));

    MythUIButtonListItem *item = m_groupList->GetItemCurrent();
    if (!item)
        return;

    QString group = item->GetText();

    for (int x = 0; x < m_siteList.count(); x++)
    {
        Bookmark *site = m_siteList.at(x);

        if (group == site->category)
        {
            MythUIButtonListItem *item = new MythUIButtonListItem(
                    m_bookmarkList, "", "", true, MythUIButtonListItem::NotChecked);
            item->SetText(site->name, "name");
            item->SetText(site->url, "url");
            if (site->isHomepage)
                item->DisplayState("yes", "homepage");
            item->SetData(qVariantFromValue(site));
            item->setChecked(site->selected ?
                    MythUIButtonListItem::FullChecked : MythUIButtonListItem::NotChecked);
        }
    }
}

uint BookmarkManager::GetMarkedCount(void)
{
    uint count = 0;

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            count++;
    }

    return count;
}

bool BookmarkManager::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {

        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
            QString label = tr("Actions");

            MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

            m_menuPopup = new MythDialogBox(label, popupStack, "actionmenu");

            if (!m_menuPopup->Create())
            {
                delete m_menuPopup;
                m_menuPopup = NULL;
                return true;
            }

            m_menuPopup->SetReturnEvent(this, "action");

            m_menuPopup->AddButton(tr("Set Homepage"), SLOT(slotSetHomepage()));
            m_menuPopup->AddButton(tr("Add Bookmark"), SLOT(slotAddBookmark()));

            if (m_bookmarkList->GetItemCurrent())
            {
                m_menuPopup->AddButton(tr("Edit Bookmark"), SLOT(slotEditBookmark()));
                m_menuPopup->AddButton(tr("Delete Bookmark"), SLOT(slotDeleteCurrent()));
                m_menuPopup->AddButton(tr("Show Bookmark"), SLOT(slotShowCurrent()));
            }

            if (GetMarkedCount() > 0)
            {
                m_menuPopup->AddButton(tr("Delete Marked"), SLOT(slotDeleteMarked()));
                m_menuPopup->AddButton(tr("Show Marked"), SLOT(slotShowMarked()));
                m_menuPopup->AddButton(tr("Clear Marked"), SLOT(slotClearMarked()));
            }
            
            m_menuPopup->AddButton(tr("Settings"), SLOT(slotSettings()));

            popupStack->AddScreen(m_menuPopup);
        }
        else if (action == "INFO")
        {
            MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();

            if (item)
            {
                Bookmark *site = qVariantValue<Bookmark*>(item->GetData());

                if (item->state() == MythUIButtonListItem::NotChecked)
                {
                    item->setChecked(MythUIButtonListItem::FullChecked);
                    if (site)
                        site->selected = true;
                }
                else
                {
                    item->setChecked(MythUIButtonListItem::NotChecked);
                    if (site)
                        site->selected = false;
                }
            }
        }
        else if (action == "DELETE")
            slotDeleteCurrent();
        else if (action == "EDIT")
            slotEditBookmark();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void BookmarkManager::slotGroupSelected(MythUIButtonListItem *item)
{
    (void) item;

    UpdateURLList();
    m_bookmarkList->Refresh();
}

void BookmarkManager::slotBookmarkClicked(MythUIButtonListItem *item)
{
    if (!item)
        return;

    Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
    if (!site)
        return;

    m_savedBookmark = *site;

    QString cmd = gCoreContext->GetSetting("WebBrowserCommand", "Internal");
    QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");
    QStringList urls;

    urls.append(site->url);

    if (cmd.toLower() == "internal")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MythScreenType *mythbrowser;
        if (urls[0].startsWith("mythflash://"))
            mythbrowser = new MythFlashPlayer(mainStack, urls);
        else
            mythbrowser = new MythBrowser(mainStack, urls);

        if (mythbrowser->Create())
        {
            connect(mythbrowser, SIGNAL(Exiting()), SLOT(slotBrowserClosed()));
            mainStack->AddScreen(mythbrowser);
        }
        else
            delete mythbrowser;
    }
    else
    {
        cmd.replace("%ZOOM%", zoom);
        cmd.replace("%URL%", urls.join(" "));

        cmd.replace("&","\\&");
        cmd.replace(";","\\;");

        GetMythMainWindow()->AllowInput(false);
        myth_system(cmd, kMSDontDisableDrawing);
        GetMythMainWindow()->AllowInput(true);

        // we need to reload the bookmarks incase the user added/deleted
        // any while in MythBrowser
        ReloadBookmarks();
    }
}

void BookmarkManager::ShowEditDialog(bool edit)
{
    Bookmark *site = NULL;

    if (edit)
    {
        MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();

        if (item && item->GetData().isValid())
        {
            site = qVariantValue<Bookmark*>(item->GetData());
            m_savedBookmark = *site;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "BookmarkManager: Something is wrong. "
                                     "Asked to edit a non existent bookmark!");
            return;
        }
    }


    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    BookmarkEditor *editor = new BookmarkEditor(&m_savedBookmark, edit, mainStack,
                                                        "bookmarkeditor");

    connect(editor, SIGNAL(Exiting()), this, SLOT(slotEditDialogExited()));

    if (editor->Create())
        mainStack->AddScreen(editor);
}

void BookmarkManager::slotEditDialogExited(void)
{
    ReloadBookmarks();
}

void BookmarkManager::ReloadBookmarks(void)
{
    GetSiteList(m_siteList);
    UpdateGroupList();

    m_groupList->MoveToNamedPosition(m_savedBookmark.category);
    UpdateURLList();

    // try to set the current item to name
    MythUIButtonListItem *item;
    for (int x = 0; x < m_bookmarkList->GetCount(); x++)
    {
        item = m_bookmarkList->GetItemAt(x);
        if (item && item->GetData().isValid())
        {
            Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
            if (site && (*site == m_savedBookmark))
            {
                m_bookmarkList->SetItemCurrent(item);
                break;
            }
        }
    }
}

void BookmarkManager::slotSettings(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    BrowserConfig *config = new BrowserConfig(mainStack, "browserconfig");

    if (config->Create())
        mainStack->AddScreen(config);
}

void BookmarkManager::slotSetHomepage(void)
{
    // Clear all homepage information
    ResetHomepageFromDB();

    // Set the homepage information for selected bookmark
    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item && item->GetData().isValid())
    {
        Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
        if (site)
            UpdateHomepageInDB(site);
    }
    ReloadBookmarks();
}

void BookmarkManager::slotAddBookmark(void)
{
    ShowEditDialog(false);
}

void BookmarkManager::slotEditBookmark(void)
{
    ShowEditDialog(true);
}

void BookmarkManager::slotDeleteCurrent(void)
{
    if (!m_bookmarkList->GetItemCurrent())
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Are you sure you want to delete the selected bookmark?");

    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            this, SLOT(slotDoDeleteCurrent(bool)));
}

void BookmarkManager::slotDoDeleteCurrent(bool doDelete)
{
    if (!doDelete)
        return;

    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item)
    {
        QString category = "";
        Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
        if (site)
        {
            category = site->category;
            RemoveFromDB(site);
        }

        GetSiteList(m_siteList);
        UpdateGroupList();

        if (category != "")
            m_groupList->MoveToNamedPosition(category);

        UpdateURLList();
    }
}

void BookmarkManager::slotDeleteMarked(void)
{
    if (GetMarkedCount() == 0)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    QString message = tr("Are you sure you want to delete the marked bookmarks?");

    MythConfirmationDialog *dialog = new MythConfirmationDialog(popupStack, message, true);

    if (dialog->Create())
        popupStack->AddScreen(dialog);

    connect(dialog, SIGNAL(haveResult(bool)),
            this, SLOT(slotDoDeleteMarked(bool)));
}

void BookmarkManager::slotDoDeleteMarked(bool doDelete)
{
    if (!doDelete)
        return;

    QString category = m_groupList->GetValue();

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            RemoveFromDB(site);
    }

    GetSiteList(m_siteList);
    UpdateGroupList();

    if (category != "")
        m_groupList->MoveToNamedPosition(category);

    UpdateURLList();
}

void BookmarkManager::slotShowCurrent(void)
{
    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item)
        slotBookmarkClicked(item);
}

void BookmarkManager::slotShowMarked(void)
{
    if (GetMarkedCount() == 0)
        return;

    MythUIButtonListItem *item = m_bookmarkList->GetItemCurrent();
    if (item && item->GetData().isValid())
    {
       Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
       m_savedBookmark = *site;
    }

    QString cmd = gCoreContext->GetSetting("WebBrowserCommand", "Internal");
    QString zoom = gCoreContext->GetSetting("WebBrowserZoomLevel", "1.0");
    QStringList urls;

    for (int x = 0; x < m_siteList.size(); x++)
    {
        Bookmark *site = m_siteList.at(x);
        if (site && site->selected)
            urls.append(site->url);
    }

    if (cmd.toLower() == "internal")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MythBrowser *mythbrowser = new MythBrowser(mainStack, urls);

        if (mythbrowser->Create())
        {
            connect(mythbrowser, SIGNAL(Exiting()), SLOT(slotBrowserClosed()));
            mainStack->AddScreen(mythbrowser);
        }
        else
            delete mythbrowser;
    }
    else
    {
        cmd.replace("%ZOOM%", zoom);
        cmd.replace("%URL%", urls.join(" "));

        cmd.replace("&","\\&");
        cmd.replace(";","\\;");

        GetMythMainWindow()->AllowInput(false);
        myth_system(cmd, kMSDontDisableDrawing);
        GetMythMainWindow()->AllowInput(true);

        // we need to reload the bookmarks incase the user added/deleted
        // any while in MythBrowser
        ReloadBookmarks();
    }
}

void BookmarkManager::slotBrowserClosed(void)
{
    // we need to reload the bookmarks incase the user added/deleted
    // any while in MythBrowser
    ReloadBookmarks();
}

void BookmarkManager::slotClearMarked(void)
{
    for (int x = 0; x < m_bookmarkList->GetCount(); x++)
    {
        MythUIButtonListItem *item = m_bookmarkList->GetItemAt(x);
        if (item)
        {
            item->setChecked(MythUIButtonListItem::NotChecked);

            Bookmark *site = qVariantValue<Bookmark*>(item->GetData());
            if (site)
                site->selected = false;
        }
    }
}

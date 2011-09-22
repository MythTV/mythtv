#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <QDir>
#include <QKeyEvent>

// myth
#include <mythcontext.h>
#include <mythcoreutil.h>
#include <mythuitext.h>
#include <mythuibutton.h>
#include <mythuicheckbox.h>
#include <mythuibuttonlist.h>
#include <mythuitextedit.h>
#include <mythmainwindow.h>

// mytharchive
#include "selectdestination.h"
#include "fileselector.h"
#include "archiveutil.h"
#include "exportnative.h"
#include "themeselector.h"

SelectDestination::SelectDestination(
    MythScreenStack *parent, bool nativeMode, QString name) :
    MythScreenType(parent, name),
    m_nativeMode(nativeMode),
    m_freeSpace(0),
    m_nextButton(NULL),
    m_prevButton(NULL),
    m_cancelButton(NULL),
    m_destinationSelector(NULL),
    m_destinationText(NULL),
    m_freespaceText(NULL),
    m_filenameEdit(NULL),
    m_findButton(NULL),
    m_createISOCheck(NULL),
    m_doBurnCheck(NULL),
    m_eraseDvdRwCheck(NULL),
    m_createISOText(NULL),
    m_doBurnText(NULL),
    m_eraseDvdRwText(NULL)
{
    m_archiveDestination.type = AD_FILE;
    m_archiveDestination.name = NULL;
    m_archiveDestination.description = NULL;
    m_archiveDestination.freeSpace = 0LL;
}

SelectDestination::~SelectDestination(void)
{
    saveConfiguration();
}


bool SelectDestination::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "selectdestination", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_createISOCheck, "makeisoimage_check", &err);
    UIUtilE::Assign(this, m_doBurnCheck, "burntodvdr_check", &err);
    UIUtilE::Assign(this, m_doBurnText, "burntodvdr_text", &err);
    UIUtilE::Assign(this, m_eraseDvdRwCheck, "erasedvdrw_check", &err);
    UIUtilE::Assign(this, m_eraseDvdRwText, "erasedvdrw_text", &err);
    UIUtilE::Assign(this, m_nextButton, "next_button", &err);
    UIUtilE::Assign(this, m_prevButton, "prev_button", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_destinationSelector, "destination_selector", &err);
    UIUtilE::Assign(this, m_destinationText, "destination_text", &err);
    UIUtilE::Assign(this, m_findButton, "find_button", &err);
    UIUtilE::Assign(this, m_filenameEdit, "filename_edit", &err);
    UIUtilE::Assign(this, m_freespaceText, "freespace_text", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'selectdestination'");
        return false;
    }

    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));

    connect(m_destinationSelector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(setDestination(MythUIButtonListItem*)));

    for (int x = 0; x < ArchiveDestinationsCount; x++)
    {
        MythUIButtonListItem *item = new 
            MythUIButtonListItem(m_destinationSelector, tr(ArchiveDestinations[x].name));
        item->SetData(qVariantFromValue(ArchiveDestinations[x].type));
    }
    connect(m_findButton, SIGNAL(Clicked()), this, SLOT(handleFind()));

    connect(m_filenameEdit, SIGNAL(LosingFocus()), this,
            SLOT(filenameEditLostFocus()));

    BuildFocusList();

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool SelectDestination::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
        {
        }
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void SelectDestination::handleNextPage()
{
    saveConfiguration();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (m_nativeMode)
    {
        ExportNative *native = new ExportNative(mainStack, this, m_archiveDestination, "ExportNative");

        if (native->Create())
            mainStack->AddScreen(native);
    }
    else
    {
        DVDThemeSelector *theme = new DVDThemeSelector(mainStack, this, m_archiveDestination, "ThemeSelector");

        if (theme->Create())
            mainStack->AddScreen(theme);
    }
}

void SelectDestination::handlePrevPage()
{
    Close();
}

void SelectDestination::handleCancel()
{
    Close();
}

void SelectDestination::loadConfiguration(void)
{
    bool bCreateISO = false;
    bool bDoBurn = false;
    bool bEraseDvdRw = false;
    QString saveFilename = "";
    int destinationType = 0;

    if (m_nativeMode)
    {
        bCreateISO = (gCoreContext->GetSetting("MythNativeCreateISO", "0") == "1");
        bDoBurn = (gCoreContext->GetSetting("MythNativeBurnDVDr", "1") == "1");
        bEraseDvdRw = (gCoreContext->GetSetting("MythNativeEraseDvdRw", "0") == "1");
        saveFilename = gCoreContext->GetSetting("MythNativeSaveFilename", "");
        destinationType = gCoreContext->GetNumSetting("MythNativeDestinationType", 0);
    }
    else
    {
        bCreateISO = (gCoreContext->GetSetting("MythBurnCreateISO", "0") == "1");
        bDoBurn = (gCoreContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
        bEraseDvdRw = (gCoreContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");
        saveFilename = gCoreContext->GetSetting("MythBurnSaveFilename", "");
        destinationType = gCoreContext->GetNumSetting("MythBurnDestinationType", 0);
    }

    m_createISOCheck->SetCheckState((bCreateISO ? MythUIStateType::Full : MythUIStateType::Off));
    m_doBurnCheck->SetCheckState((bDoBurn ? MythUIStateType::Full : MythUIStateType::Off));
    m_eraseDvdRwCheck->SetCheckState((bEraseDvdRw ? MythUIStateType::Full : MythUIStateType::Off));
    m_filenameEdit->SetText(saveFilename);

    if (destinationType < 0 || destinationType >= m_destinationSelector->GetCount())
        destinationType = 0;
    m_destinationSelector->SetItemCurrent(destinationType);
}

void SelectDestination::saveConfiguration(void)
{
    if (m_nativeMode)
    {
        gCoreContext->SaveSetting("MythNativeCreateISO",
            (m_createISOCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythNativeBurnDVDr",
            (m_doBurnCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythNativeEraseDvdRw",
            (m_eraseDvdRwCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythNativeSaveFilename", m_filenameEdit->GetText());
        gCoreContext->SaveSetting("MythNativeDestinationType", m_destinationSelector->GetCurrentPos());
    }
    else
    {
        gCoreContext->SaveSetting("MythBurnCreateISO",
            (m_createISOCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythBurnBurnDVDr",
            (m_doBurnCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythBurnEraseDvdRw",
            (m_eraseDvdRwCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gCoreContext->SaveSetting("MythBurnSaveFilename", m_filenameEdit->GetText());
        gCoreContext->SaveSetting("MythBurnDestinationType", m_destinationSelector->GetCurrentPos());
    }
}

void SelectDestination::setDestination(MythUIButtonListItem* item)
{
    if (!item)
        return;

    int itemNo = item->GetData().value<ARCHIVEDESTINATION>();

    if (itemNo < 0 || itemNo > ArchiveDestinationsCount - 1)
        itemNo = 0;

    m_destinationText->SetText(tr(ArchiveDestinations[itemNo].description));

    m_archiveDestination = ArchiveDestinations[itemNo];

    switch(itemNo)
    {
        case AD_DVD_SL:
        case AD_DVD_DL:
            m_filenameEdit->Hide();
            m_findButton->Hide();
            m_eraseDvdRwCheck->Hide();
            m_eraseDvdRwText->Hide();
            m_doBurnCheck->Show();
            m_doBurnText->Show();
            break;
        case AD_DVD_RW:
            m_filenameEdit->Hide();
            m_findButton->Hide();
            m_eraseDvdRwCheck->Show();
            m_eraseDvdRwText->Show();
            m_doBurnCheck->Show();
            m_doBurnText->Show();
            break;
        case AD_FILE:
            long long dummy;
            ArchiveDestinations[itemNo].freeSpace = 
                    getDiskSpace(m_filenameEdit->GetText(), dummy, dummy);

            m_filenameEdit->Show();
            m_findButton->Show();
            m_eraseDvdRwCheck->Hide();
            m_eraseDvdRwText->Hide();
            m_doBurnCheck->Hide();
            m_doBurnText->Hide();
            break;
    }

    // update free space
    if (ArchiveDestinations[itemNo].freeSpace != -1)
    {
        m_freespaceText->SetText(formatSize(ArchiveDestinations[itemNo].freeSpace, 2));
        m_freeSpace = ArchiveDestinations[itemNo].freeSpace / 1024;
    }
    else
    {
        m_freespaceText->SetText(tr("Unknown"));
        m_freeSpace = 0;
    }

    BuildFocusList();
}

void SelectDestination::handleFind(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    FileSelector *selector = new
            FileSelector(mainStack, NULL, FSTYPE_DIRECTORY, m_filenameEdit->GetText(), "*.*");

    connect(selector, SIGNAL(haveResult(QString)),
            this, SLOT(fileFinderClosed(QString)));

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void SelectDestination::fileFinderClosed(QString filename)
{
    if (filename != "")
    {
        m_filenameEdit->SetText(filename);
        filenameEditLostFocus();
    }
}

void SelectDestination::filenameEditLostFocus()
{
    long long dummy;
    m_archiveDestination.freeSpace = getDiskSpace(m_filenameEdit->GetText(), dummy, dummy);

    // if we don't get a valid freespace value it probably means the file doesn't
    // exist yet so try looking up the freespace for the parent directory 
    if (m_archiveDestination.freeSpace == -1)
    {
        QString dir = m_filenameEdit->GetText();
        int pos = dir.lastIndexOf('/');
        if (pos > 0)
            dir = dir.left(pos);
        else
            dir = "/";

        m_archiveDestination.freeSpace = getDiskSpace(dir, dummy, dummy);
    }

    if (m_archiveDestination.freeSpace != -1)
    {
        m_freespaceText->SetText(formatSize(m_archiveDestination.freeSpace, 2));
        m_freeSpace = m_archiveDestination.freeSpace;
    }
    else
    {
        m_freespaceText->SetText(tr("Unknown"));
        m_freeSpace = 0;
    }
}

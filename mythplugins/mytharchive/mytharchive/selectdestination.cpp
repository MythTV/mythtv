#include <unistd.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <QDir>
#include <QKeyEvent>

// myth
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuitextedit.h>
#include <libmythui/mythmainwindow.h>

// mytharchive
#include "selectdestination.h"
#include "fileselector.h"
#include "archiveutil.h"
#include "exportnative.h"
#include "themeselector.h"

SelectDestination::SelectDestination(MythScreenStack *parent, bool nativeMode, QString name)
                  : MythScreenType(parent, name)
{
    m_nativeMode = nativeMode;
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
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'selectdestination'");
        return false;
    }

    m_nextButton->SetText(tr("Next"));
    connect(m_nextButton, SIGNAL(Clicked()), this, SLOT(handleNextPage()));

    m_prevButton->SetText(tr("Previous"));
    connect(m_prevButton, SIGNAL(Clicked()), this, SLOT(handlePrevPage()));

    m_cancelButton->SetText(tr("Cancel"));
    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(handleCancel()));

    connect(m_destinationSelector, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(setDestination(MythUIButtonListItem*)));

    for (int x = 0; x < ArchiveDestinationsCount; x++)
    {
        MythUIButtonListItem *item = new 
            MythUIButtonListItem(m_destinationSelector, tr(ArchiveDestinations[x].name));
        item->SetData(qVariantFromValue(ArchiveDestinations[x].type));
    }
    m_findButton->SetText(tr("Find..."));
    connect(m_findButton, SIGNAL(Clicked()), this, SLOT(handleFind()));

    connect(m_filenameEdit, SIGNAL(LosingFocus()), this,
            SLOT(filenameEditLostFocus()));

    if (!BuildFocusList())
        VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    loadConfiguration();

    SetFocusWidget(m_nextButton);

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
        ThemeSelector *theme = new ThemeSelector(mainStack, this, m_archiveDestination, "ThemeSelector");

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
        bCreateISO = (gContext->GetSetting("MythNativeCreateISO", "0") == "1");
        bDoBurn = (gContext->GetSetting("MythNativeBurnDVDr", "1") == "1");
        bEraseDvdRw = (gContext->GetSetting("MythNativeEraseDvdRw", "0") == "1");
        saveFilename = gContext->GetSetting("MythNativeSaveFilename", "");
        destinationType = gContext->GetNumSetting("MythNativeDestinationType", 0);
    }
    else
    {
        bCreateISO = (gContext->GetSetting("MythBurnCreateISO", "0") == "1");
        bDoBurn = (gContext->GetSetting("MythBurnBurnDVDr", "1") == "1");
        bEraseDvdRw = (gContext->GetSetting("MythBurnEraseDvdRw", "0") == "1");
        saveFilename = gContext->GetSetting("MythBurnSaveFilename", "");
        destinationType = gContext->GetNumSetting("MythBurnDestinationType", 0);
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
        gContext->SaveSetting("MythNativeCreateISO",
            (m_createISOCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythNativeBurnDVDr",
            (m_doBurnCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythNativeEraseDvdRw",
            (m_eraseDvdRwCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythNativeSaveFilename", m_filenameEdit->GetText());
        gContext->SaveSetting("MythNativeDestinationType", m_destinationSelector->GetCurrentPos());
    }
    else
    {
        gContext->SaveSetting("MythBurnCreateISO",
            (m_createISOCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythBurnBurnDVDr",
            (m_doBurnCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythBurnEraseDvdRw",
            (m_eraseDvdRwCheck->GetCheckState() == MythUIStateType::Full ? "1" : "0"));
        gContext->SaveSetting("MythBurnSaveFilename", m_filenameEdit->GetText());
        gContext->SaveSetting("MythBurnDestinationType", m_destinationSelector->GetCurrentPos());
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

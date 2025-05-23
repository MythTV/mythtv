#include <cstdlib>
#include <iostream>
#include <unistd.h>

// qt
#include <QDir>
#include <QKeyEvent>

// myth
#include <libmythbase/filesysteminfo.h>
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/stringutil.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythuibutton.h>
#include <libmythui/mythuibuttonlist.h>
#include <libmythui/mythuicheckbox.h>
#include <libmythui/mythuitext.h>
#include <libmythui/mythuitextedit.h>

// mytharchive
#include "archiveutil.h"
#include "exportnative.h"
#include "fileselector.h"
#include "selectdestination.h"
#include "themeselector.h"

SelectDestination::~SelectDestination(void)
{
    saveConfiguration();
}


bool SelectDestination::Create(void)
{
    // Load the theme for this screen
    bool foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "selectdestination", this);
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

    connect(m_nextButton, &MythUIButton::Clicked, this, &SelectDestination::handleNextPage);
    connect(m_prevButton, &MythUIButton::Clicked, this, &SelectDestination::handlePrevPage);
    connect(m_cancelButton, &MythUIButton::Clicked, this, &SelectDestination::handleCancel);

    connect(m_destinationSelector, &MythUIButtonList::itemSelected,
            this, &SelectDestination::setDestination);

    for (const auto & dest : ArchiveDestinations)
    {
        auto *item = new
            MythUIButtonListItem(m_destinationSelector, tr(dest.name));
        item->SetData(QVariant::fromValue(dest.type));
    }
    connect(m_findButton, &MythUIButton::Clicked, this, &SelectDestination::handleFind);

    connect(m_filenameEdit, &MythUIType::LosingFocus, this,
            &SelectDestination::filenameEditLostFocus);

    BuildFocusList();

    SetFocusWidget(m_nextButton);

    loadConfiguration();

    return true;
}

bool SelectDestination::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        const QString& action = actions[i];
        handled = true;

        if (action == "MENU")
        {
        }
        else
        {
            handled = false;
        }
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
        auto *native = new ExportNative(mainStack, this, m_archiveDestination, "ExportNative");

        if (native->Create())
            mainStack->AddScreen(native);
    }
    else
    {
        auto *theme = new DVDThemeSelector(mainStack, this, m_archiveDestination, "ThemeSelector");

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

    size_t itemNo = item->GetData().value<ARCHIVEDESTINATION>();

    if (itemNo > ArchiveDestinations.size() - 1)
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
            ArchiveDestinations[itemNo].freeSpace = 
                    FileSystemInfo(QString(), m_filenameEdit->GetText()).getFreeSpace();

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
        m_freespaceText->SetText(StringUtil::formatKBytes(ArchiveDestinations[itemNo].freeSpace, 2));
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

    auto *selector = new
            FileSelector(mainStack, nullptr, FSTYPE_DIRECTORY, m_filenameEdit->GetText(), "*.*");

    connect(selector, qOverload<QString>(&FileSelector::haveResult),
            this, &SelectDestination::fileFinderClosed);

    if (selector->Create())
        mainStack->AddScreen(selector);
}

void SelectDestination::fileFinderClosed(const QString& filename)
{
    if (filename != "")
    {
        m_filenameEdit->SetText(filename);
        filenameEditLostFocus();
    }
}

void SelectDestination::filenameEditLostFocus()
{
    auto fsInfo = FileSystemInfo(QString(), m_filenameEdit->GetText());

    // if we don't get a valid freespace value it probably means the file doesn't
    // exist yet so try looking up the freespace for the parent directory 
    if (!fsInfo.refresh())
    {
        QString dir = m_filenameEdit->GetText();
        int pos = dir.lastIndexOf('/');
        if (pos > 0)
            dir = dir.left(pos);
        else
            dir = "/";

        fsInfo = FileSystemInfo(QString(), dir);
    }

    if (fsInfo.refresh())
    {
        m_archiveDestination.freeSpace = fsInfo.getFreeSpace();
        m_freespaceText->SetText(StringUtil::formatKBytes(m_archiveDestination.freeSpace, 2));
        m_freeSpace = m_archiveDestination.freeSpace;
    }
    else
    {
        m_archiveDestination.freeSpace = -1;
        m_freespaceText->SetText(tr("Unknown"));
        m_freeSpace = 0;
    }
}

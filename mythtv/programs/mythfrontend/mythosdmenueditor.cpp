#define MYTHOSDMENUEDITOR_CPP

/* QT includes */
#include <QString>
#include <QListIterator>

/* MythTV includes */
#include "mythverbose.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythosdmenueditor.h"
#include "tvosdmenuentry.h"

/* Static data members */
TVState MythOSDMenuEditor::WatchingLiveTV = kState_WatchingLiveTV;
TVState MythOSDMenuEditor::WatchingPreRecorded = kState_WatchingPreRecorded;
TVState MythOSDMenuEditor::WatchingVideo = kState_WatchingVideo;
TVState MythOSDMenuEditor::WatchingDVD = kState_WatchingDVD;

Q_DECLARE_METATYPE(TVState*)

MythOSDMenuEditor::MythOSDMenuEditor(MythScreenStack *parent, const char *name)
    : MythScreenType(parent, name), m_menuEntryList(NULL), m_states(NULL),
        m_categories(NULL), m_doneButton(NULL), m_tvstate(WatchingLiveTV)
{
    m_menuEntryList = new TVOSDMenuEntryList();
}

MythOSDMenuEditor::~MythOSDMenuEditor()
{
    if (m_menuEntryList)
    {
        m_menuEntryList->UpdateDB();
        delete m_menuEntryList;
    }
}

void MythOSDMenuEditor::updateCategoryList(bool active)
{
    m_categories->Reset();
    QListIterator<TVOSDMenuEntry*> cm = m_menuEntryList->GetIterator();
    while (cm.hasNext())
    {
        TVOSDMenuEntry *entry = cm.next();
        MythUIButtonListItem::CheckState checked =
                                            MythUIButtonListItem::NotChecked;
        int enable_entry = entry->GetEntry(m_tvstate);
        if (enable_entry > 0)
        {
            checked = MythUIButtonListItem::FullChecked;
        }
        if (enable_entry > -1)
        {
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_categories, entry->GetDescription(),
                                      0, true, checked);
            item->SetData(qVariantFromValue(entry));
        }
    }
}

bool MythOSDMenuEditor::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("config-ui.xml", "osdmenueditor", this);

    if (!foundtheme)
        return false;

    bool err = false;
    UIUtilE::Assign(this, m_states, "states", &err);
    UIUtilE::Assign(this, m_categories, "categories", &err);
    UIUtilE::Assign(this, m_doneButton, "done", &err);

    if (err)
    {
        VERBOSE(VB_IMPORTANT, "Cannot load screen 'MythOSDMenuEditor'");
        return false;
    }

    connect(m_doneButton, SIGNAL(Clicked()), SLOT(Close()));
    connect(m_states, SIGNAL(itemSelected(MythUIButtonListItem*)),
            SLOT(slotStateChanged(MythUIButtonListItem*)));
    connect(m_categories, SIGNAL(itemClicked(MythUIButtonListItem*)),
            SLOT(slotToggleItem(MythUIButtonListItem*)));

    MythUIButtonListItem* item = new MythUIButtonListItem(m_states, "Live TV");
    item->setDrawArrow(true);
    item->SetData(qVariantFromValue(&WatchingLiveTV));

    item = new MythUIButtonListItem(m_states, "Recorded TV");
    item->setDrawArrow(true);
    item->SetData(qVariantFromValue(&WatchingPreRecorded));

    item = new MythUIButtonListItem(m_states, "Video");
    item->setDrawArrow(true);
    item->SetData(qVariantFromValue(&WatchingVideo));

    item = new MythUIButtonListItem(m_states, "DVD/BD");
    item->setDrawArrow(true);
    item->SetData(qVariantFromValue(&WatchingDVD));

    updateCategoryList(false);

    BuildFocusList();

    return true;
}

void MythOSDMenuEditor::slotStateChanged(MythUIButtonListItem *item)
{
    TVState *state = qVariantValue<TVState*>(item->GetData());

    if (state == NULL || *state == m_tvstate) return;
    m_tvstate = *state;
    updateCategoryList(false);

}

void MythOSDMenuEditor::slotToggleItem(MythUIButtonListItem *item)
{

    TVOSDMenuEntry *entry =
        qVariantValue<TVOSDMenuEntry *>(item->GetData());

    if (entry == NULL) return;

    int  enable_entry = 0;
    MythUIButtonListItem::CheckState entry_checkstate =
        MythUIButtonListItem::NotChecked;
    if (item->state() == MythUIButtonListItem::NotChecked)
    {
        enable_entry = 1;
        entry_checkstate = MythUIButtonListItem::FullChecked;
    }

    item->setChecked(entry_checkstate);
    entry->UpdateEntry(enable_entry, m_tvstate);
}

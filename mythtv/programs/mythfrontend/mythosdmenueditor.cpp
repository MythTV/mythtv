#define MYTHOSDMENUEDITOR_CPP

/* QT includes */
#include <qnamespace.h>
#include <qstringlist.h>
#include <qapplication.h>
#include <qdom.h>
#include <qbuttongroup.h>
#include <qimage.h>
#include <qevent.h>
#include <qdir.h>
#include <qstring.h>

/* MythTV includes */
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythmainwindow.h"
#include "myththemebase.h"
#include "mythuibuttonlist.h"
#include "mythuibutton.h"
#include "mythosdmenueditor.h"
#include "tvosdmenuentry.h"


using namespace std;

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

MythOSDMenuEditor::~MythOSDMenuEditor() {
    m_menuEntryList->UpdateDB();
    delete m_menuEntryList;
}

void MythOSDMenuEditor::updateCategoryList(bool active)
{
    QString state_name = (active)? "entrycolors" : "inactivecolors";

    m_categories->Reset();
    QListIterator<TVOSDMenuEntry*> cm = m_menuEntryList->GetIterator();
    while(cm.hasNext())
    {
        TVOSDMenuEntry *entry = cm.next();
        MythUIButtonListItem::CheckState checked =
                                            MythUIButtonListItem::NotChecked;
        int enable_entry = entry->GetEntry(m_tvstate);
        QString entry_state = "inactive";
        if (enable_entry > 0)
        {
            checked = MythUIButtonListItem::FullChecked;
            if (active)
                entry_state = "active";
        }
        if (enable_entry > -1)
        {
            MythUIButtonListItem *item =
                new MythUIButtonListItem(m_categories, entry->GetDescription(),
                                      0, true, checked);
            item->SetData(qVariantFromValue(entry));
            item->DisplayState(entry_state, state_name);
        }
    }
}

bool MythOSDMenuEditor::Create()
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("osdedit-ui.xml", "osdmenueditor", this);

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

    m_doneButton->SetText("Return");

    connect(m_doneButton, SIGNAL(Clicked()), this, SLOT(Close()));
    connect(m_states, SIGNAL(itemSelected(MythUIButtonListItem*)),
 	        this, SLOT(slotStateChanged(MythUIButtonListItem*)));
    connect(m_categories, SIGNAL(itemClicked(MythUIButtonListItem*)),
            this, SLOT(slotToggleItem(MythUIButtonListItem*)));

    if (!BuildFocusList())
      VERBOSE(VB_IMPORTANT, "Failed to build a focuslist. Something is wrong");

    MythUIButtonListItem* item = new MythUIButtonListItem(m_states, "Live TV");
    item->SetData(qVariantFromValue(&WatchingLiveTV));

    item = new MythUIButtonListItem(m_states, "Recorded TV");
    item->SetData(qVariantFromValue(&WatchingPreRecorded));

    item = new MythUIButtonListItem(m_states, "Video");
    item->SetData(qVariantFromValue(&WatchingVideo));

    item = new MythUIButtonListItem(m_states, "DVD");
    item->SetData(qVariantFromValue(&WatchingDVD));

    updateCategoryList(false);
    SetFocusWidget(m_states);

    return true;
}

bool MythOSDMenuEditor::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
        return true;

    QStringList actions;
    bool handled = false;

    gContext->GetMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
         handled = true;

        if (action == "RIGHT" || action == "LEFT")
            moveRightOrLeft(action);
        else if (action == "UP")
            handled = moveUpOrDown("up");
        else if (action == "DOWN")
            handled = moveUpOrDown("down");
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

bool MythOSDMenuEditor::moveUpOrDown(QString direction)
{
    MythUIButtonList *buttonlist = (MythUIButtonList *)GetFocusWidget();
    int current_pos = buttonlist->GetCurrentPos();
    int count = buttonlist->GetCount();
    bool handled = true;
    if (direction == "up" && current_pos == 0)
        buttonlist->SetItemCurrent(count - 1);
    else if (direction == "down" && current_pos == (count - 1))
        buttonlist->SetItemCurrent(0);
    else
        handled = false;

    return handled;
}

void MythOSDMenuEditor::moveRightOrLeft(QString direction)
{
    QString entry_state = "inactive";
    MythUIType *widget = GetFocusWidget();
    if (widget == m_categories)
    {
        if (direction == "LEFT")
            SetFocusWidget(m_states);
        else
            SetFocusWidget(m_doneButton);
    }
    else if (widget == m_states)
    {
        if (direction == "LEFT")
            SetFocusWidget(m_doneButton);
        else
        {
            entry_state = "active";
            SetFocusWidget(m_categories);
        }
    }
    else if (widget == m_doneButton)
    {
        if (direction == "LEFT")
        {
            entry_state = "active";
            SetFocusWidget(m_categories);
        }
        else
            SetFocusWidget(m_states);
    }

    updateCategoryList(entry_state == "active");

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

    QString entry_state = "inactive";
    int  enable_entry = 0;
    MythUIButtonListItem::CheckState entry_checkstate =
        MythUIButtonListItem::NotChecked;
    if (item->state() == MythUIButtonListItem::NotChecked)
    {
        enable_entry = 1;
        entry_checkstate = MythUIButtonListItem::FullChecked;
        entry_state = "active";
    }

    item->setChecked(entry_checkstate);
    item->DisplayState(entry_state, "entrycolors");
    entry->UpdateEntry(enable_entry, m_tvstate);
}

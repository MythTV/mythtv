#ifndef MYTHOSDMENUEDITOR_H
#define MYTHOSDMENUEDITOR_H

#include "mythscreentype.h"
#include "tv.h"
//#include "tvosdmenuentry.h"

class MythUIButtonList;
class MythUIButton;
class TVOSDMenuEntryList;

class MythOSDMenuEditor : public MythScreenType
{
    Q_OBJECT

  public:
    MythOSDMenuEditor(MythScreenStack *parent, const char *name);
    ~MythOSDMenuEditor();

    bool Create(void);

  private:
    TVOSDMenuEntryList *m_menuEntryList;

    static TVState WatchingLiveTV;
    static TVState WatchingPreRecorded;
    static TVState WatchingVideo;
    static TVState WatchingDVD;

    MythUIButtonList *m_states;
    MythUIButtonList *m_categories;
    MythUIButton *m_doneButton;
    TVState m_tvstate;

    void updateCategoryList(bool active = true);

  private slots:
    void slotStateChanged(MythUIButtonListItem *item);
    void slotToggleItem(MythUIButtonListItem *item);
};

#endif

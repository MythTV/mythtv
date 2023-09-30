#ifndef SEARCHVIEW_H_
#define SEARCHVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// MythTV
#include <libmythui/mythscreentype.h>

// mythmusic
#include "musiccommon.h"

class MythUIButtonList;
class MythUIText;
class MythUITextEdit;

class SearchView : public MusicCommon
{
    Q_OBJECT
  public:
    SearchView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~SearchView(void) override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MusicCommon

    void ShowMenu(void) override; // MusicCommon

  protected:
    void customEvent(QEvent *event) override; // MusicCommon
    void updateTracksList(void);

  protected slots:
    void fieldSelected(MythUIButtonListItem *item);
    void criteriaChanged(void);

    static void trackClicked(MythUIButtonListItem *item);
    static void trackVisible(MythUIButtonListItem *item);

  private:
    int                  m_playTrack    {-1};
    MythUIButtonList    *m_fieldList    {nullptr};
    MythUITextEdit      *m_criteriaEdit {nullptr};
    MythUIText          *m_matchesText  {nullptr};
    MythUIButtonList    *m_tracksList   {nullptr};
};

#endif

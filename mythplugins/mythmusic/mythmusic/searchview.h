#ifndef SEARCHVIEW_H_
#define SEARCHVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// mythui
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIButtonList;
class MythUIText;
class MythUITextEdit;

class SearchView : public MusicCommon
{
    Q_OBJECT
  public:
    SearchView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~SearchView(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

    virtual void ShowMenu(void);

  protected:
    void customEvent(QEvent *event);
    void updateTracksList(void);

  protected slots:
    void fieldSelected(MythUIButtonListItem *item);
    void criteriaChanged(void);

    void trackClicked(MythUIButtonListItem *item);
    void trackVisible(MythUIButtonListItem *item);

  private:
    bool                 m_playTrack;
    MythUIButtonList    *m_fieldList;
    MythUITextEdit      *m_criteriaEdit;
    MythUIText          *m_matchesText;
    MythUIButtonList    *m_tracksList;
};

#endif

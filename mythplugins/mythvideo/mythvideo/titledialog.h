#ifndef TITLEDIALOG_H_
#define TITLEDIALOG_H_

// QT headers
#include <QTimer>
#include <Q3Socket>
#include <QList>

// Myth headers
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

// Mythui headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythlistbutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuitextedit.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuicheckbox.h>

// Mythvideo headers
#include "dvdinfo.h"

class TitleDialog : public MythScreenType
{
    Q_OBJECT

  public:

    TitleDialog(MythScreenStack *parent,
                const QString &name,
                Q3Socket *a_socket,
                QString d_name,
                QList<DVDTitleInfo*> *titles);
   ~TitleDialog();

    bool Create(void);

  public slots:

    void showCurrentTitle();
    void viewTitle();
    void nextTitle();
    void prevTitle();
    void gotoTitle(uint title_number);
    void toggleTitle();
    void changeName();
    void setAudio(MythListButtonItem *);
    void setQuality(MythListButtonItem *);
    void setSubTitle(MythListButtonItem *);
    void toggleAC3();
    void ripTitles();

  private:
    QTimer                  *m_checkDvdTimer;
    QString                  m_discName;
    QList<DVDTitleInfo*>    *m_dvdTitles;
    DVDTitleInfo            *m_currentTitle;
    Q3Socket                *m_socketToMtd;

    MythUITextEdit      *m_nameEdit;
    MythListButton      *m_audioList;
    MythListButton      *m_qualityList;
    MythListButton      *m_subtitleList;
    MythUICheckBox      *m_ripCheck;
    MythUICheckBox      *m_ripacthreeCheck;
    MythUIText          *m_playlengthText;
    MythUIText          *m_numbtitlesText;
    MythUIButton        *m_viewButton;
    MythUIButton        *m_nexttitleButton;
    MythUIButton        *m_prevtitleButton;
    MythUIButton        *m_ripawayButton;
};

#endif

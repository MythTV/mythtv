#ifndef TITLEDIALOG_H_
#define TITLEDIALOG_H_

#include <QList>

#include <mythscreentype.h>

#include "dvdinfo.h"

class QTcpSocket;

class MythUIButtonList;
class MythUIButtonListItem;
class MythUIText;
class MythUITextEdit;
class MythUIButton;
class MythUICheckBox;

class TitleDialog : public MythScreenType
{
    Q_OBJECT

  public:

    TitleDialog(MythScreenStack *lparent,
                QString lname,
                QTcpSocket *a_socket,
                QString d_name,
                QList<DVDTitleInfo*> *titles);

    bool Create();

  public slots:

    void showCurrentTitle();
    void viewTitle();
    void nextTitle();
    void prevTitle();
    void gotoTitle(uint title_number);
    void toggleTitle();
    void changeName();
    void setAudio(MythUIButtonListItem *);
    void setQuality(MythUIButtonListItem *);
    void setSubTitle(MythUIButtonListItem *);
    void toggleAC3();
    void ripTitles();

  private:
    QString                  m_discName;
    QList<DVDTitleInfo *>   *m_dvdTitles;
    DVDTitleInfo            *m_currentTitle;
    QTcpSocket              *m_socketToMtd;

    MythUITextEdit      *m_nameEdit;
    MythUIButtonList      *m_audioList;
    MythUIButtonList      *m_qualityList;
    MythUIButtonList      *m_subtitleList;
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

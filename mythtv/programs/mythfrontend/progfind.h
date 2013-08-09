#ifndef PROGFIND_H_
#define PROGFIND_H_

// Qt
#include <QDateTime>
#include <QEvent>

// MythTV
#include "mythscreentype.h"
#include "programinfo.h"
#include "mythdialogbox.h"
#include "playercontext.h"

// mythfrontend
#include "schedulecommon.h"

class TV;
class MythUIText;
class MythUIButtonList;

void RunProgramFinder(TV *player = NULL, bool embedVideo = false, bool allowEPG = true);

class ProgFinder : public ScheduleCommon
{
    Q_OBJECT
  public:
    explicit ProgFinder(MythScreenStack *parentStack, bool allowEPG = true,
               TV *player = NULL, bool embedVideo = false);
    virtual ~ProgFinder();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  private slots:
    void alphabetListItemSelected(MythUIButtonListItem *item);
    void showListTakeFocus(void);
    void timesListTakeFocus(void);
    void timesListLosingFocus(void);

    void showGuide();
    void select();
    void customEdit();
    void upcoming();
    void previous();
    void details();
    void quickRecord();

    void customEvent(QEvent *e);
    void updateInfo(void);
    void getInfo(bool toggle = false);
    void edit(void);

  protected:
    typedef QMap<QString,QString> ShowName;

    virtual void Init(void);

    virtual void initAlphabetList(void);
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

    void ShowMenu(void);
    void getShowNames(void);
    void updateShowList();
    void updateTimesList();
    void selectShowData(QString, int);

    ShowName m_showNames;

    QString m_searchStr;
    QString m_currentLetter;

    TV  *m_player;
    bool m_embedVideo;
    bool m_allowEPG;
    bool m_allowKeypress;

    ProgramList m_showData;
    ProgramList m_schedList;

    InfoMap m_infoMap;

    MythUIButtonList *m_alphabetList;
    MythUIButtonList *m_showList;
    MythUIButtonList *m_timesList;

    MythUIText       *m_searchText;
    MythUIText       *m_help1Text;
    MythUIText       *m_help2Text;
};

class JaProgFinder : public ProgFinder
{
  public:
    explicit JaProgFinder(MythScreenStack *parentStack, bool gg = false,
                 TV *player = NULL, bool embedVideo = false);

  protected:
    virtual void initAlphabetList();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

  private:
    static const QChar searchChars[];
    int numberOfSearchChars;
};

class HeProgFinder : public ProgFinder
{
  public:
    explicit HeProgFinder(MythScreenStack *parentStack, bool gg = false,
                 TV *player = NULL, bool embedVideo = false);

  protected:
    virtual void initAlphabetList();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

  private:
    static const QChar searchChars[];
    int numberOfSearchChars;
};
///////////////////////////////
class RuProgFinder : public ProgFinder
{
  public:
    explicit RuProgFinder(MythScreenStack *parentStack, bool gg = false, 
                       TV *player = NULL, bool embedVideo = false);
                       
  protected:
    virtual void initAlphabetList();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);
                                             
  private:
    static const QChar searchChars[];
    int numberOfSearchChars;
};
///////////////////////////////////

class SearchInputDialog : public MythTextInputDialog
{
  Q_OBJECT

  public:
    SearchInputDialog(MythScreenStack *parent, const QString &defaultValue);

    bool Create(void);

  signals:
    void valueChanged(QString);

  private slots:
    void editChanged(void);
};

#endif

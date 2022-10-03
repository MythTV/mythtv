#ifndef PROGFIND_H_
#define PROGFIND_H_

// Qt
#include <QDateTime>
#include <QEvent>

// MythTV
#include "libmythbase/programinfo.h"
#include "libmythtv/playercontext.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythscreentype.h"

// MythFrontend
#include "schedulecommon.h"

class TV;
class MythUIText;
class MythUIButtonList;

void RunProgramFinder(TV *player = nullptr, bool embedVideo = false, bool allowEPG = true);

class ProgFinder : public ScheduleCommon
{
    Q_OBJECT
  public:
    explicit ProgFinder(MythScreenStack *parentStack, bool allowEPG = true,
               TV *player = nullptr, bool embedVideo = false);
    ~ProgFinder() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  private slots:
    void alphabetListItemSelected(MythUIButtonListItem *item);
    void showListTakeFocus(void);
    void timesListTakeFocus(void);
    void timesListLosingFocus(void);

    void ShowGuide() const override; // ScheduleCommon
    void select();

    void customEvent(QEvent *e) override; // ScheduleCommon
    void updateInfo(void);

  protected:
    using ShowName = QMap<QString,QString>;

    void Init(void) override; // MythScreenType

    virtual void initAlphabetList(void);
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);
    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

    void ShowMenu(void) override; // MythScreenType
    void getShowNames(void);
    void updateShowList();
    void updateTimesList();
    void selectShowData(QString progTitle, int newCurShow);

    ShowName m_showNames;

    QString m_searchStr;
    QString m_currentLetter;

    TV  *m_player                    {nullptr};
    bool m_embedVideo                {false};
    bool m_allowEPG                  {true};
    bool m_allowKeypress             {true};

    ProgramList m_showData;
    ProgramList m_schedList;

    InfoMap m_infoMap;

    MythUIButtonList *m_alphabetList {nullptr};
    MythUIButtonList *m_showList     {nullptr};
    MythUIButtonList *m_timesList    {nullptr};

    MythUIText       *m_searchText   {nullptr};
    MythUIText       *m_help1Text    {nullptr};
    MythUIText       *m_help2Text    {nullptr};
};

class JaProgFinder : public ProgFinder
{
  public:
    explicit JaProgFinder(MythScreenStack *parentStack, bool gg = false,
                 TV *player = nullptr, bool embedVideo = false)
        : ProgFinder(parentStack, gg, player, embedVideo) {}

  protected:
    void initAlphabetList() override; // ProgFinder
    bool formatSelectedData(QString &data) override; // ProgFinder
    bool formatSelectedData(QString &data, int charNum) override; // ProgFinder
    void restoreSelectedData(QString &data) override; // ProgFinder
    void whereClauseGetSearchData(QString &where, MSqlBindings &bindings) override; // ProgFinder

  private:
    static const std::vector<QChar> kSearchChars;
};

class HeProgFinder : public ProgFinder
{
  public:
    explicit HeProgFinder(MythScreenStack *parentStack, bool gg = false,
                 TV *player = nullptr, bool embedVideo = false)
        : ProgFinder(parentStack, gg, player, embedVideo) {}

  protected:
    void initAlphabetList() override; // ProgFinder
    bool formatSelectedData(QString &data) override; // ProgFinder
    bool formatSelectedData(QString &data, int charNum) override; // ProgFinder
    void restoreSelectedData(QString &data) override; // ProgFinder
    void whereClauseGetSearchData(QString &where, MSqlBindings &bindings) override; // ProgFinder

  private:
    static const std::vector<QChar> kSearchChars;
};
///////////////////////////////
class RuProgFinder : public ProgFinder
{
  public:
    explicit RuProgFinder(MythScreenStack *parentStack, bool gg = false, 
                       TV *player = nullptr, bool embedVideo = false)
        : ProgFinder(parentStack, gg, player, embedVideo) {}
                       
  protected:
    void initAlphabetList() override; // ProgFinder
    bool formatSelectedData(QString &data) override; // ProgFinder
    bool formatSelectedData(QString &data, int charNum) override; // ProgFinder
    void restoreSelectedData(QString &data) override; // ProgFinder
    void whereClauseGetSearchData(QString &where, MSqlBindings &bindings) override; // ProgFinder
                                             
  private:
    static const std::vector<QChar> kSearchChars;
};
///////////////////////////////////

class SearchInputDialog : public MythTextInputDialog
{
  Q_OBJECT

  public:
    SearchInputDialog(MythScreenStack *parent, const QString &defaultValue)
        : MythTextInputDialog(parent, "", FilterNone, false, defaultValue) {}

    bool Create(void) override; // MythTextInputDialog

  signals:
    void valueChanged(QString);

  private slots:
    void editChanged(void);
};

#endif

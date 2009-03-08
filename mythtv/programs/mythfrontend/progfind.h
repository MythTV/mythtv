#ifndef PROGFIND_H_
#define PROGFIND_H_

// qt
#include <QDateTime>
#include <QEvent>
#include <QKeyEvent>
#include <QPaintEvent>

// myth
#include "mythscreentype.h"
#include "programlist.h"
#include "mythdialogbox.h"

class ProgramInfo;
class TV;
class MythUIText;
class MythUIButtonList;

MPUBLIC void RunProgramFind(bool thread = false, bool ggActive = false);

class ProgFinder : public MythScreenType
{
    Q_OBJECT
  public:
    ProgFinder(MythScreenStack *parentStack, bool gg = false);
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
    void details();
    void quickRecord();

    void customEvent(QEvent *e);
    void updateInfo(void);
    void getInfo(bool toggle = false);

  protected:
    virtual void Init(void);

    virtual void initAlphabetList(void);
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

    void showMenu(void);
    void updateShowList();
    void updateTimesList();
    void selectShowData(QString, int);

    QString m_searchStr;
    
    bool m_ggActive;
    bool m_allowKeypress;

    ProgramList m_showData;
    ProgramList m_schedList;

    QString m_dateFormat;
    QString m_timeFormat;

    QMap<QString, QString> m_infoMap;

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
    JaProgFinder(MythScreenStack *parentStack, bool gg=false);

  protected:
    virtual void initAlphabetList();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

  private:
    static const char* searchChars[];
    int numberOfSearchChars;
};

class HeProgFinder : public ProgFinder
{
  public:
    HeProgFinder(MythScreenStack *parentStack, bool gg=false);

  protected:
    virtual void initAlphabetList();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(QString &where, MSqlBindings &bindings);

  private:
    static const char* searchChars[];
    int numberOfSearchChars;
};

class MPUBLIC SearchInputDialog : public MythTextInputDialog
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

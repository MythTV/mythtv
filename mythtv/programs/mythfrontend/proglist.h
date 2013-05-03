#ifndef PROGLIST_H_
#define PROGLIST_H_

// Qt headers
#include <QDateTime>
#include <QString>

// MythTV headers
#include "programinfo.h" // for ProgramList
#include "schedulecommon.h"
#include "proglist_helpers.h"

enum ProgListType {
    plUnknown = 0,
    plTitle = 1,
    plTitleSearch,
    plKeywordSearch,
    plPeopleSearch,
    plPowerSearch,
    plSQLSearch,
    plNewListings,
    plMovies,
    plCategory,
    plChannel,
    plTime,
    plRecordid,
    plStoredSearch,
    plPreviouslyRecorded
};

class ProgLister : public ScheduleCommon
{
    friend class PhrasePopup;
    friend class TimePopup;
    friend class PowerSearchPopup;
    friend class EditPowerSearchPopup;

    Q_OBJECT

  public:
    ProgLister(MythScreenStack *parent, ProgListType pltype,
               const QString &view, const QString &extraArg);
    explicit ProgLister(MythScreenStack *parent, uint recid = 0,
                        const QString &title = QString());
    ~ProgLister();

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);
    void customEvent(QEvent *);

  protected slots:
    void HandleSelected(MythUIButtonListItem *item);
    void HandleVisible(MythUIButtonListItem *item);
    void HandleClicked(void);

    void DeleteOldEpisode(bool ok);
    void DeleteOldSeries(bool ok);
    void RecordSelected(void);

    void SetViewFromList(QString item);
    void SetViewFromTime(QDateTime searchTime);

    void EditScheduled(void) { ScheduleCommon::EditScheduled(GetCurrent()); }
    void EditCustom(void)    { ScheduleCommon::EditCustom(GetCurrent());    }

    void ShowDetails(void)   { ScheduleCommon::ShowDetails(GetCurrent());   }
    void ShowGuide(void);
    void ShowUpcoming(void);
    void ShowDeleteRuleMenu(void);
    void ShowDeleteOldEpisodeMenu(void);
    void ShowChooseViewMenu(void);

  private:
    void Load(void);

    void FillViewList(const QString &view);
    void FillItemList(bool restorePosition, bool updateDisp = true);

    void ClearCurrentProgramInfo(void);
    void UpdateDisplay(const ProgramInfo *selected = NULL);
    void RestoreSelection(const ProgramInfo *selected, int selectedOffset);
    void UpdateButtonList(void);
    void UpdateKeywordInDB(const QString &text, const QString &oldValue);

    virtual void ShowMenu(void); // MythScreenType
    void ShowDeleteItemMenu(void);
    void ShowDeleteOldSeriesMenu(void);
    void ShowOldRecordedMenu(void);

    void SwitchToPreviousView(void);
    void SwitchToNextView(void);

    typedef enum { kTimeSort, kPrevTitleSort, kTitleSort, } SortBy;
    SortBy GetSortBy(void) const;
    void SortList(SortBy sortby, bool reverseSort);

    ProgramInfo *GetCurrent(void);
    const ProgramInfo *GetCurrent(void) const;

    bool PowerStringToSQL(
        const QString &qphrase, QString &output, MSqlBindings &bindings) const;

  private:
    ProgListType      m_type;
    uint              m_recid;
    QString           m_title;
    QString           m_extraArg;
    QDateTime         m_startTime;
    QDateTime         m_searchTime;
    QString           m_channelOrdering;

    RecSearchType     m_searchType;

    QString           m_view;
    int               m_curView;
    QStringList       m_viewList;
    QStringList       m_viewTextList;

    ProgramList       m_itemList;
    ProgramList       m_itemListSave;
    ProgramList       m_schedList;

    QStringList       m_typeList;
    QStringList       m_genreList;
    QStringList       m_stationList;

    bool              m_allowEvents;
    bool              m_titleSort;
    bool              m_reverseSort;
    bool              m_useGenres;

    MythUIText       *m_schedText;
    MythUIText       *m_curviewText;
    MythUIText       *m_positionText;
    MythUIButtonList *m_progList;
    MythUIText       *m_messageText;

    bool              m_allowViewDialog;
};

#endif

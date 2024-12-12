#ifndef PROGLIST_H_
#define PROGLIST_H_

// Qt headers
#include <QDateTime>
#include <QString>

// MythTV headers
#include "libmythbase/programinfo.h" // for ProgramList

// MythFrontend
#include "proglist_helpers.h"
#include "schedulecommon.h"

class TV;

enum ProgListType : std::uint8_t {
    plUnknown = 0,
    plTitle = 1,
    plTitleSearch = 2,
    plKeywordSearch = 3,
    plPeopleSearch = 4,
    plPowerSearch = 5,
    plSQLSearch = 6,
    plNewListings = 7,
    plMovies = 8,
    plCategory = 9,
    plChannel = 10,
    plTime = 11,
    plRecordid = 12,
    plStoredSearch = 13,
    plPreviouslyRecorded = 14
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
               QString view, QString extraArg,
               QDateTime selectedTime = QDateTime());
    ProgLister(MythScreenStack *parent, TV* player,
               ProgListType pltype, const QString & extraArg);
    explicit ProgLister(MythScreenStack *parent, uint recid = 0,
                        QString title = QString());
    ~ProgLister(void) override;

    static void * RunProgramList(void *player, ProgListType pltype,
                                 const QString & extraArg);

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

  protected slots:
    void Close(void) override; // MythScreenType

    void HandleSelected(MythUIButtonListItem *item);
    void HandleVisible(MythUIButtonListItem *item);

    void DeleteOldEpisode(bool ok);
    void DeleteOldSeries(bool ok);

    void SetViewFromList(const QString& item);
    void SetViewFromTime(QDateTime searchTime);

    void ShowDeleteRuleMenu(void);
    void ShowDeleteOldEpisodeMenu(void);
    void ShowChooseViewMenu(void);
    void ShowOldRecordedMenu(void);

  private:
    void Load(void) override; // MythScreenType

    void FillViewList(const QString &view);
    void FillItemList(bool restorePosition, bool updateDisp = true);

    void ClearCurrentProgramInfo(void);
    void UpdateDisplay(const ProgramInfo *selected = nullptr);
    void RestoreSelection(const ProgramInfo *selected, int selectedOffset);
    void UpdateButtonList(void);
    void UpdateKeywordInDB(const QString &text, const QString &oldValue);

    void ShowMenu(void) override; // MythScreenType
    void ShowDeleteItemMenu(void);
    void ShowDeleteOldSeriesMenu(void);

    void SwitchToPreviousView(void);
    void SwitchToNextView(void);

    enum SortBy : std::uint8_t { kTimeSort, kPrevTitleSort, kTitleSort, };
    SortBy GetSortBy(void) const;
    void SortList(SortBy sortby, bool reverseSort);

    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

    static bool PowerStringToSQL(
        const QString &qphrase, QString &output, MSqlBindings &bindings) ;

  private:
    ProgListType      m_type;
    uint              m_recid           {0};
    QString           m_title;
    QString           m_extraArg;
    QDateTime         m_startTime;
    QDateTime         m_searchTime;
    QDateTime         m_selectedTime;
    QString           m_channelOrdering;

    RecSearchType     m_searchType      {kNoSearch};

    QString           m_view;
    int               m_curView         {-1};
    QStringList       m_viewList;
    QStringList       m_viewTextList;

    ProgramList       m_itemList;
    ProgramList       m_itemListSave;
    ProgramList       m_schedList;

    QStringList       m_typeList;
    QStringList       m_genreList;
    QStringList       m_stationList;

    bool              m_allowEvents     {true};
    bool              m_titleSort       {false};
    bool              m_reverseSort     {false};
    bool              m_useGenres       {false};

    MythUIText       *m_schedText       {nullptr};
    MythUIText       *m_curviewText     {nullptr};
    MythUIText       *m_positionText    {nullptr};
    MythUIButtonList *m_progList        {nullptr};
    MythUIText       *m_messageText     {nullptr};

    bool              m_allowViewDialog {true};

    TV               *m_player          {nullptr};
};

#endif

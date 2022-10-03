/*
 * Copyright (c) 2017 Peter G Bennett <pbennett@mythtv.org>
 *
 * This file is part of MythTV.
 *
 * MythTV is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as
 * published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * MythTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MythTV. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PREVRECLIST_H_
#define PREVRECLIST_H_

// MythTV
#include "libmythbase/programinfo.h"

// mythfrontend
#include "schedulecommon.h"

class MythMenu;

class PrevRecordedList : public ScheduleCommon
{
    Q_OBJECT
  public:
    explicit PrevRecordedList(MythScreenStack *parent, uint recid = 0,
                        QString title = QString());
    ~PrevRecordedList() override;
    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *e) override; // MythScreenType
    void customEvent(QEvent *event) override; // ScheduleCommon

  protected slots:
    void ShowMenu(void) override; // MythScreenType
    void ShowItemMenu(void);
    void updateInfo(void);
    void showListLoseFocus(void);
    void showListTakeFocus(void);
    void DeleteOldEpisode(bool ok);
    void AllowRecord(void);
    void PreventRecord(void);
    void DeleteOldSeries(bool ok);
    void ShowDeleteOldSeriesMenu(void);
    void ShowDeleteOldEpisodeMenu(void);

  protected:
    void Init(void) override; // MythScreenType
    void Load(void) override; // MythScreenType

    bool LoadTitles(void);
    bool LoadDates(void);
    void UpdateTitleList(void);
    void UpdateShowList(void);
    void UpdateList(MythUIButtonList *bnList,
          ProgramList *progData, bool isShows) const;
    void LoadShowsByTitle(void);
    void LoadShowsByDate(void);

    ProgramInfo *GetCurrentProgram(void) const override; // ScheduleCommon

    // Left hand list - titles or dates
    ProgramList m_titleData;
    MythUIButtonList *m_titleList      {nullptr};
    // Right hand list - show details
    ProgramList m_showData;
    MythUIButtonList *m_showList       {nullptr};
    // MythUIStateType *m_groupByState {nullptr};

    MythUIText       *m_curviewText    {nullptr};
    MythUIText       *m_help1Text      {nullptr};
    MythUIText       *m_help2Text      {nullptr};

    InfoMap m_infoMap;

    bool              m_titleGroup     {true};
    bool              m_reverseSort    {false};
    bool              m_allowEvents    {true};
    uint              m_recid          {0};
    QString           m_title;
    QString           m_where;
    bool              m_loadShows      {false};
};

#endif

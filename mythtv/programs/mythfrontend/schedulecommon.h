#ifndef SCHEDULE_COMMON_H
#define SCHEDULE_COMMON_H

// Qt
#include <QObject>
#include <QEvent>

// MythTV
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreentype.h"

class ProgramInfo;
class RecordingInfo;

class ScheduleCommon : public MythScreenType
{
    Q_OBJECT

  protected:
    ScheduleCommon(MythScreenStack *parent, const QString &name)
        : MythScreenType(parent, name) {};
   ~ScheduleCommon() override = default;

    static void ShowUpcoming(const QString &title, const QString &seriesid) ;
    static void EditScheduled(ProgramInfo *pginfo);
    static void EditScheduled(RecordingInfo *recinfo);
    static void MakeOverride(RecordingInfo *recinfo);

    void customEvent(QEvent *event) override; // MythUIType
    virtual ProgramInfo *GetCurrentProgram(void) const { return nullptr; };

  public slots:
    virtual void ShowDetails(void) const;

  protected slots:
    virtual void EditRecording(bool may_watch_now = false);
    virtual void EditRecording(MythUIButtonListItem* /*item*/);
    virtual void QuickRecord(void);
    virtual void ShowPrevious(void) const;
    virtual void ShowPrevious(uint ruleid, const QString &title) const;
    virtual void ShowUpcoming(void) const;
    virtual void ShowUpcomingScheduled(void) const;
    virtual void ShowChannelSearch(void) const;
    virtual void ShowGuide(void) const;
    virtual void EditScheduled(void);
    virtual void EditCustom(void);

  private:
    static bool IsFindApplicable(const RecordingInfo &recInfo) ;

};

#endif

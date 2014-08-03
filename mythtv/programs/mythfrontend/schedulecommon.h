#ifndef SCHEDULE_COMMON_H
#define SCHEDULE_COMMON_H

// QT
#include <QObject>
#include <QEvent>

// libmythui
#include "mythscreentype.h"
#include "mythmainwindow.h"

class ProgramInfo;
class RecordingInfo;

class ScheduleCommon : public MythScreenType
{
    Q_OBJECT

  protected:
    ScheduleCommon(MythScreenStack *parent, const QString &name)
        : MythScreenType(parent, name) {};
   ~ScheduleCommon() {};

    void ShowUpcoming(const QString &title, const QString &seriesid) const;
    void EditScheduled(ProgramInfo *pginfo);
    void EditScheduled(RecordingInfo *recinfo);
    void MakeOverride(RecordingInfo *recinfo);

    virtual void customEvent(QEvent*);
    virtual ProgramInfo *GetCurrentProgram(void) const { return NULL; };

  public slots:
    virtual void ShowDetails(void) const;

  protected slots:
    virtual void EditRecording(void);
    virtual void QuickRecord(void);
    virtual void ShowPrevious(void) const;
    virtual void ShowUpcoming(void) const;
    virtual void ShowUpcomingScheduled(void) const;
    virtual void ShowChannelSearch(void) const;
    virtual void ShowGuide(void) const;
    virtual void EditScheduled(void);
    virtual void EditCustom(void);

  private:
    bool IsFindApplicable(const RecordingInfo &recInfo) const;

};

#endif

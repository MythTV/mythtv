#ifndef MANUALSCHEDULE_H_
#define MANUALSCHEDULE_H_

#include <QDateTime>
#include <QStringList>

#include "mythscreentype.h"

class QTimer;
class ProgramInfo;

class MythUIButton;
class MythUIButtonList;
class MythUISpinBox;
class MythUITextEdit;

class ManualSchedule : public MythScreenType
{
    Q_OBJECT
  public:

    ManualSchedule(MythScreenStack *parent);
   ~ManualSchedule(void) {};

    bool Create(void);

  protected slots:
    void dateChanged(void);
    void hourRollover(void);
    void minuteRollover(void);
    void recordClicked(void);
    void scheduleCreated(int);

  private:
    void connectSignals();
    void disconnectSignals();
    int daysahead;
    int prev_weekday;

    QStringList m_chanids;

    MythUITextEdit *m_title;

    MythUIButtonList *m_channel;
    MythUIButtonList *m_startdate;

    MythUISpinBox *m_starthour;
    MythUISpinBox *m_startminute;
    MythUISpinBox *m_duration;

    MythUIButton *m_recordButton;
    MythUIButton *m_cancelButton;

    QDateTime m_nowDateTime;
    QDateTime m_startDateTime;
    QString m_categoryString;
    QString m_startString;
    QString m_chanidString;

    QString m_dateformat;
    QString m_timeformat;
    QString m_shortdateformat;
};

#endif

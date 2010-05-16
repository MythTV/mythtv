#ifndef MANUALSCHEDULE_H_
#define MANUALSCHEDULE_H_

// ANSI C
#include <stdint.h> // for [u]int[32,64]_t

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
    
    int m_daysahead;

    QList<uint32_t> m_chanids;

    MythUITextEdit *m_titleEdit;

    MythUIButtonList *m_channelList;
    MythUIButtonList *m_startdateList;

    MythUISpinBox *m_starthourSpin;
    MythUISpinBox *m_startminuteSpin;
    MythUISpinBox *m_durationSpin;

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

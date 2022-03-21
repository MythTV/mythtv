#ifndef MANUALSCHEDULE_H_
#define MANUALSCHEDULE_H_

// C++
#include <cstdint> // for [u]int[32,64]_t

#include <QDateTime>
#include <QStringList>

#include "libmythui/mythscreentype.h"

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

    explicit ManualSchedule(MythScreenStack *parent);
   ~ManualSchedule(void) override = default;

    bool Create(void) override; // MythScreenType

  protected slots:
    void dateChanged(void);
    void hourRollover(void);
    void minuteRollover(void);
    void recordClicked(void);
    void scheduleCreated(int ruleid);

  private:
    void connectSignals();
    void disconnectSignals();
    
    int             m_daysahead       {0};

    QList<uint32_t> m_chanids;

    MythUITextEdit *m_titleEdit       {nullptr};

    MythUIButtonList *m_channelList   {nullptr};
    MythUIButtonList *m_startdateList {nullptr};

    MythUISpinBox *m_starthourSpin   {nullptr};
    MythUISpinBox *m_startminuteSpin {nullptr};
    MythUISpinBox *m_durationSpin    {nullptr};

    MythUIButton *m_recordButton     {nullptr};
    MythUIButton *m_cancelButton     {nullptr};

    QDateTime m_nowDateTime;
    QDateTime m_startDateTime;
    QString m_categoryString;
    QString m_startString;
    QString m_chanidString;

};

#endif

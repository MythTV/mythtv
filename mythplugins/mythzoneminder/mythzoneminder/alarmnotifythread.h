#ifndef ALARMNOTIFYTHREAD_H
#define ALARMNOTIFYTHREAD_H

// Qt headers

// MythTV headers
#include <libmythbase/mthread.h>

// zm
#include "zmdefines.h"

class AlarmNotifyThread : public MThread
{
  protected:
    AlarmNotifyThread(void);

    static AlarmNotifyThread *m_alarmNotifyThread;

  public:
    ~AlarmNotifyThread(void) override;

    static AlarmNotifyThread *get(void);
    void stop(void);

  protected:
    void run(void) override; // MThread

  private:
    volatile bool m_stop{false};
};

#endif // ALARMNOTIFYTHREAD_H

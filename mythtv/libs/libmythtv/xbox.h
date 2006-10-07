#ifndef XBOX_H_
#define XBOX_H_

#include <qobject.h>
#include <qtimer.h>

#include "mythexp.h"

class MPUBLIC XBox : public QObject
{
    Q_OBJECT
  public:
    XBox(void);

    void GetSettings(void);

  protected slots:
    void CheckRec(void);

  private:
    QTimer *timer;

    int RecCheck;

    QString RecordingLED;
    QString DefaultLED;
    QString PhaseCache;
    QString BlinkBIN;
};

#endif

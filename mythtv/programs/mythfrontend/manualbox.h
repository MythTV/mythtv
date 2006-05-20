#ifndef MANUALBOX_H_
#define MANUALBOX_H_

#include <qdatetime.h>
#include <qhbox.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "tv.h"

#include <pthread.h>

class TV;
class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class ManualBox : public MythDialog
{
    Q_OBJECT
  public:

    ManualBox(MythMainWindow *parent, const char *name = 0);
   ~ManualBox(void);
   
  signals:
    void dismissWindow();

  protected slots:
    void timeout(void);
    void refreshTimeout(void);
    void textChanged(const QString &);
    void startClicked(void);
    void stopClicked(void);

  protected:
     bool eventFilter(QObject *o, QEvent *e);

  private:
    QHBox *m_boxframe;
    QLabel *m_pixlabel;
    MythLineEdit *m_title;
    MythLineEdit *m_subtitle;
    MythSpinBox *m_duration;
    MythPushButton *m_startButton;
    MythPushButton *m_stopButton;

    void killPlayer(void);
    void startPlayer(void);

    QString m_descString;
    QString m_categoryString;
    QString m_startString;
    QString m_chanidString;

    QString m_lastStarttime;
    QString m_lastChanid;
    
    bool m_wasRecording;
    TV *m_tv;
    QTimer *m_timer;
    QTimer *m_refreshTimer;

    bool tvstarting;
};

#endif

#ifndef MANUALSCHEDULE_H_
#define MANUALSCHEDULE_H_

#include <qdatetime.h>
#include <qhbox.h>
#include "libmyth/mythwidgets.h"
#include "tv.h"
#include "NuppelVideoPlayer.h"
#include "yuv2rgb.h"

#include <pthread.h>

class QListViewItem;
class QLabel;
class QProgressBar;
class NuppelVideoPlayer;
class RingBuffer;
class QTimer;
class ProgramInfo;

class ManualSchedule : public MythDialog
{
    Q_OBJECT
  public:

    ManualSchedule(MythMainWindow *parent, const char *name = 0);
   ~ManualSchedule(void);
   
  signals:
    void dismissWindow();

  protected slots:
    void textChanged(void);
    void hourChanged(void);
    void minuteChanged(void);
    void channelChanged(void);
    void durationChanged(void);
    void dateChanged(void);
    void saveClicked(void);
    void exitClicked(void);
    void cancelClicked(void);
    void saveScheduledRecording(void);


  private:
    int daysahead;
    int prev_weekday;

    QHBox *m_boxframe;
    QLabel *m_pixlabel;
    MythRemoteLineEdit *m_title;
    MythComboBox *m_channel;
    QStringList m_chanids;
    MythRemoteLineEdit *m_subtitle;
    MythRemoteLineEdit *m_description;
    MythSpinBox *m_duration;
    MythPushButton *m_saveButton;
    MythPushButton *m_exitButton;
    MythPushButton *m_cancelButton;
    MythComboBox *m_startdate;
    MythSpinBox *m_starthour;
    MythSpinBox *m_startminute;

    MythRemoteLineEdit *m_descString;

    QDateTime m_nowDateTime;
    QDateTime m_startDateTime;
    QString m_categoryString;
    QString m_startString;
    QString m_chanidString;

    QString m_lastStarttime;
    QString m_lastChanid;
    QString m_lastDuration;

    QString m_lastChannel;
    QString m_lastSubtitle;

    QString dateformat;
    QString timeformat;
    QString shortdateformat;

    
    bool m_wasRecording;
};

#endif

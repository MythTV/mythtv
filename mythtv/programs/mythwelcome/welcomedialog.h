#ifndef WELCOMEDIALOG_H_
#define WELCOMEDIALOG_H_

#include <qdatetime.h>

#include "mythdialogs.h"
#include "remoteutil.h"

class WelcomeDialog : public MythThemedDialog
{

  Q_OBJECT

  public:

    WelcomeDialog(MythMainWindow *parent,
                       QString window_name,
                       QString theme_filename,
                       const char* name = 0);
    ~WelcomeDialog();

    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);
    void wireUpTheme();
    DialogCode exec(void);
    
  protected slots:
    void startFrontendClick(void);
    void startFrontend(void);
    void updateAll(void);
    void updateStatus(void);
    void updateScreen(void);
    void closeDialog();
    void updateTime();  
    void showPopup();
    void donePopup(int);
    void cancelPopup();
    void shutdownNow();
    void runEPGGrabber(void);
    void lockShutdown();
    void unlockShutdown();
    bool updateRecordingList(void);
    bool updateScheduledList(void);
     
  private:
    void updateStatusMessage(void);
    bool checkConnectionToServer(void);
    void runMythFillDatabase(void);
        
    UITextType* getTextType(QString name);
    
    MythPopupBox *popup;
    
    //
    //  GUI stuff
    //
    UITextType          *m_status_text;
    UITextType          *m_recording_text;
    UITextType          *m_scheduled_text;
    UITextType          *m_warning_text;
    UITextType          *m_time_text;
    UITextType          *m_date_text;
    
    UITextButtonType    *m_startfrontend_button;
    
    QTimer         *m_updateStatusTimer;
    QTimer         *m_updateScreenTimer;
    QTimer         *m_timeTimer;

    QString        m_installDir;
    QString        m_timeFormat;
    QString        m_dateFormat;
    bool           m_isRecording;
    bool           m_hasConflicts;
    bool           m_bWillShutdown;
    int            m_secondsToShutdown;
    QDateTime      m_nextRecordingStart;
    int            m_preRollSeconds;
    int            m_idleWaitForRecordingTime;
    uint           m_screenTunerNo;
    uint           m_screenScheduledNo;
    uint           m_statusListNo;
    QStringList    m_statusList;
     
    typedef struct
    {
        QString channel, title, subtitle;
        QDateTime startTime, endTime;
    } ProgramDetail;

    QPtrList<TunerStatus>    m_tunerList;
    QPtrList<ProgramDetail>  m_scheduledList;        

    QMutex      m_RecListUpdateMuxtex;
    bool        m_pendingRecListUpdate;

    bool pendingRecListUpdate() const           { return m_pendingRecListUpdate; }
    void setPendingRecListUpdate(bool newState) { m_pendingRecListUpdate = newState; }
       
    QMutex      m_SchedUpdateMuxtex;
    bool        m_pendingSchedUpdate;

    bool pendingSchedUpdate() const           { return m_pendingSchedUpdate; }
    void setPendingSchedUpdate(bool newState) { m_pendingSchedUpdate = newState; }
	   
};

#endif

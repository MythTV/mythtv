#ifndef WELCOMEDIALOG_H_
#define WELCOMEDIALOG_H_

#include <iostream>
using namespace std;

#include <qdatetime.h>
#include "mythdialogs.h"

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
    int exec(void);
    
  protected slots:
    void startFrontendClick(void);
    void startFrontend(void);
    void updateAll(void);
    void updateStatus(void);
    void updateScreen(void);
    void closeDialog();
    void sendAllowShutdown(void);
    void updateTime();  
    void showPopup();
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
        
    UITextType* WelcomeDialog::getTextType(QString name);
    
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
    bool           m_isRecording;
    bool           m_hasConflicts;
    bool           m_bSentAllowShutdown;
    bool           m_bWillShutdown;
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

    typedef struct
    {
        int     id;
        bool    isRecording;
        ProgramDetail program;
    } TunerStatus;
    
    QPtrList<TunerStatus>    m_tunerList;
    QPtrList<ProgramDetail>  m_scheduledList;        
};

#endif

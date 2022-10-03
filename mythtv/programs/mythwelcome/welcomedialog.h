#ifndef WELCOMEDIALOG_H_
#define WELCOMEDIALOG_H_

// qt
#include <QDateTime>

// myth
#include "libmythbase/programinfo.h"
#include "libmythtv/tvremoteutil.h"
#include "libmythui/mythdialogbox.h"
#include "libmythui/mythscreentype.h"
#include "libmythui/mythuibutton.h"
#include "libmythui/mythuitext.h"

class GroupSetting;

class WelcomeDialog : public MythScreenType
{

  Q_OBJECT

  public:

    WelcomeDialog(MythScreenStack *parent, const char *name);
    ~WelcomeDialog() override;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void customEvent(QEvent *e) override; // MythUIType

  protected slots:
    void startFrontendClick(void);
    void startFrontend(void);
    void updateAll(void);
    void updateStatus(void);
    void updateScreen(void);
    void closeDialog(void);
    void ShowMenu(void) override; // MythScreenType
    void shutdownNow(void);
    void runEPGGrabber(void);
    void lockShutdown(void);
    void unlockShutdown(void);
    bool updateRecordingList(void);
    bool updateScheduledList(void);

  private:
    void updateStatusMessage(void);
    bool checkConnectionToServer(void);
    void checkAutoStart(void);
    static void runMythFillDatabase(void);
    static void ShowSettings(GroupSetting* screen);

    //
    //  GUI stuff
    //
    MythUIText    *m_statusText               { nullptr };
    MythUIText    *m_recordingText            { nullptr };
    MythUIText    *m_scheduledText            { nullptr };
    MythUIText    *m_warningText              { nullptr };

    MythUIButton  *m_startFrontendButton      { nullptr };

    MythDialogBox *m_menuPopup                { nullptr };

    QTimer        *m_updateStatusTimer        { nullptr }; // audited ref #5318
    QTimer        *m_updateScreenTimer        { nullptr }; // audited ref #5318

    QString        m_appBinDir;
    bool           m_isRecording              { false };
    bool           m_hasConflicts             { false };
    bool           m_willShutdown             { false };
    int            m_secondsToShutdown        { -1 };
    QDateTime      m_nextRecordingStart;
    std::chrono::seconds m_preRollSeconds     { 0s };
    std::chrono::seconds m_idleWaitForRecordingTime { 0s };
    std::chrono::seconds m_idleTimeoutSecs    { 0s };
    uint           m_screenTunerNo            { 0 };
    uint           m_screenScheduledNo        { 0 };
    uint           m_statusListNo             { 0 };
    QStringList    m_statusList;
    bool           m_frontendIsRunning        { false };

    std::vector<TunerStatus> m_tunerList;
    std::vector<ProgramInfo> m_scheduledList;

    QMutex      m_recListUpdateMuxtex;
    bool        m_pendingRecListUpdate        { false };

    bool pendingRecListUpdate() const           { return m_pendingRecListUpdate; }
    void setPendingRecListUpdate(bool newState) { m_pendingRecListUpdate = newState; }

    QMutex      m_schedUpdateMuxtex;
    bool        m_pendingSchedUpdate          { false };

    bool pendingSchedUpdate() const           { return m_pendingSchedUpdate; }
    void setPendingSchedUpdate(bool newState) { m_pendingSchedUpdate = newState; }

};

#endif

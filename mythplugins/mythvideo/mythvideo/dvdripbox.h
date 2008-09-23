#ifndef DVDRIPBOX_H_
#define DVDRIPBOX_H_

// QT headers
#include <QRegExp>
#include <QTimer>
#include <QThread>
#include <Q3Socket>
#include <QVector>

// Mythui headers
#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuibutton.h>
#include <mythtv/libmythui/mythuitext.h>
#include <mythtv/libmythui/mythuiprogressbar.h>

// Mythvideo headers
#include "dvdinfo.h"

class MTDJob : public QObject
{
    Q_OBJECT

    //
    //  A little class that stores
    //  data about jobs running on the
    //  mtd
    //

  public:
    MTDJob();
    MTDJob(const QString &a_name_);

    void init();

    //
    //  Set
    //

    void setNumber(int a_number) { job_number = a_number; }
    void SetName(const QString &a_name);
    void setActivity(const QString &an_act);
    void setOverall(double a_number) { overall_progress = a_number; }
    void setSubjob(double a_number);
    void setCancelled(bool yes_or_no) { cancelled = yes_or_no; }

    //
    //  Get
    //

    int     getNumber() { return job_number; }
    QString getName() { return job_name; }
    QString getActivity() { return current_activity; }
    double  getOverall() { return overall_progress; }
    double  getSubjob() { return subjob_progress; }

  signals:

    void    toggledCancelled();

  private:

    int      job_number;
    QString  job_name;
    QString  current_activity;
    double   overall_progress;
    double   subjob_progress;
    bool     cancelled;
};

class DVDRipBox : public MythScreenType
{
    Q_OBJECT

  public:
    typedef QVector<int> IntVector;

    DVDRipBox(MythScreenStack *parent, const QString &name, const QString &dev);
    ~DVDRipBox(void);

    bool Create(void);

    void connectToMtd();

    enum RipState { RIPSTATE_UNKNOWN=0, RIPSTATE_NOCONNECT, RIPSTATE_NOJOBS,
                   RIPSTATE_HAVEJOBS };

  public slots:
    void connectionError(int error_id);
    void connectionMade();
    void connectionClosed();
    void readFromServer();
    void parseTokens(QStringList tokens);
    void sendToServer(const QString &some_text);
    void startStatusPolling();
    void stopStatusPolling();
    void pollStatus();
    void handleStatus(QStringList tokens);
    void handleMedia(QStringList tokens);
    void setOverallJobStatus(int job_number, double status, QString title);
    void setSubJobStatus(int job_number, double status, QString subjob_string);
    void adjustJobs(uint new_number);
    void nextJob();
    void prevJob();
    void goToJob(int which_job);
    void showCurrentJob();
    void goRipScreen();
    void checkDisc();
    void cancelJob();
    void toggleCancel();
    void ExitingRipScreen();

  private:
    void Init();
    void createSocket();

    Q3Socket         *m_clientSocket;
    QTimer           *m_statusTimer;
    bool             m_triedMTD;
    bool             m_connected;
    bool             m_firstRun;
    bool             m_haveDisc;
    bool             m_firstDiscFound;
    bool             m_blockMediaRequests;
    QList<MTDJob*>    m_jobs;
    uint             m_jobCount;
    int              m_currentJob;
    bool             m_ignoreCancels;
    RipState         m_context;

    QString          m_device;       ///> The most recent usable DVD drive
    DVDInfo          *m_dvdInfo;
    QTimer           *m_discCheckingTimer;

    MythUIText        *m_warningText;
    MythUIText        *m_overallText;
    MythUIText        *m_jobText;
    MythUIText        *m_numjobsText;
    MythUIProgressBar *m_overallProgress;
    MythUIProgressBar *m_jobProgress;
    MythUIButton      *m_ripscreenButton;
    MythUIButton      *m_cancelButton;
    MythUIButton      *m_nextjobButton;
    MythUIButton      *m_prevjobButton;
};


#endif

/*
	dvdripbox.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    header for the main interface screen
*/

#ifndef DVDRIPBOX_H_
#define DVDRIPBOX_H_

#include <qsqldatabase.h>
#include <qregexp.h>
#include <qtimer.h>
#include <qptrlist.h>
#include <qthread.h>

#include <mythtv/mythwidgets.h>
#include <mythtv/dialogbox.h>


#include "dvdinfo.h"

class MTDJob 
{
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
    
    void setName(const QString &a_name){job_name = a_name;}
    void setActivity(const QString &an_act){current_activity = an_act;}
    void setOverall(double a_number){overall_progress = a_number;}
    void setSubjob(double a_number){subjob_progress = a_number;}
    
    //
    //  Get
    //
    
    QString getName(){return job_name;}
    QString getActivity(){return current_activity;}
    double getOverall(){return overall_progress;}
    double getSubjob(){return subjob_progress;}
          
  private:
  
    QString  job_name;
    QString  current_activity;
    double   overall_progress;
    double   subjob_progress;
};

class DVDRipBox : public MythThemedDialog
{

  Q_OBJECT

  public:

    typedef QValueVector<int> IntVector;
    
    DVDRipBox(QSqlDatabase *ldb,
              MythMainWindow *parent, QString window_name,
              QString theme_filename, const char *name = 0);

    ~DVDRipBox(void);

    void connectToMtd(bool try_to_run_mtd);
    void keyPressEvent(QKeyEvent *e);
    
  public slots:
  
    void connectionError(int error_id);
    void connectionMade();
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
     
  private:

    void    wireUpTheme();
    void    createSocket();

    QSocket          *client_socket;
    QSqlDatabase     *db;
    QTimer           *status_timer;
    bool             tried_mtd;
    bool             connected;
    bool             first_time_through;
    bool             have_disc;
    bool             first_disc_found;
    bool             block_media_requests;
    QPtrList<MTDJob> jobs;
    uint             numb_jobs;
    int              current_job;

    DVDInfo             *dvd_info;
    QTimer              *disc_checking_timer;

    //
    //  Theme-related "widgets"
    //

    UITextType       *warning_text;
    UITextType       *overall_text;
    UITextType       *job_text;
    UITextType       *numb_jobs_text;
    UITextType       *nodvd_text;
    UIStatusBarType  *overall_status;
    UIStatusBarType  *job_status;
    UIPushButtonType *nextjob_button;
    UIPushButtonType *prevjob_button;
    UITextButtonType *ripscreen_button;
};


#endif

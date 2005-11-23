#ifndef JOBDIALOG_H_
#define JOBDIALOG_H_
/*
	jobdialog.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <mythtv/mythdialogs.h>
#include <mythtv/uitypes.h>

class MfdInfo;
class MfdInterface;

class JobDialog : public MythThemedDialog
{

    Q_OBJECT

  public:

    JobDialog(
                MythMainWindow *parent, 
                QString window_name,
                QString theme_filename,
                MfdInfo *l_mfd_info,
                MfdInterface *l_mfd_interface
             );
                    
    void keyPressEvent(QKeyEvent *e);

    ~JobDialog();

  public slots:
  
    void    checkJobs();
    void    cancelJob();
    void    nextJob();
    void    prevJob();

  private:
  
    void wireUpTheme();
    void tryToShowFirstJob();
    void setNumbJobs(int job_number, int numb_total);

    UITextType          *description_text;

    UITextType          *major_description;
    UITextType          *minor_description;
    
    UIStatusBarType     *major_progress;
    UIStatusBarType     *minor_progress;

    UITextButtonType    *cancel_button;

    UITextType          *numb_jobs_text;
    UIPushButtonType    *nextjob_button;
    UIPushButtonType    *prevjob_button;

    MfdInterface       *mfd_interface;
    MfdInfo            *mfd_info;
    int                 current_job;

};



#endif

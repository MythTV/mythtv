#ifndef JOBTHREAD_H_
#define JOBTHREAD_H_
/*
	jobthread.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the mtd threads that actually do things

*/

#include <qthread.h>
#include <qstringlist.h>
#include <qfile.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

#include "fileobs.h"

class MTD;

class JobThread : public QThread
{

    //
    //  A base class for all mtd threads
    //

  public:
  
    JobThread(MTD *owner, const QString &start_string, int nice_priority);
    virtual void run();
    bool keepGoing();
    
    QString getJobName(){return job_name;}
    QString getSubName(){return subjob_name;}
    double  getProgress(){return overall_progress;}
    double  getSubProgress(){return subjob_progress;}
    
    void    problem(const QString &a_problem){problem_string = a_problem;}
    QString getProblem(){return problem_string;}
    QString getJobString(){return job_string;}

    
  protected:
  
    QString job_name;
    QString subjob_name;
    double  overall_progress;
    double  subjob_progress;
    MTD     *parent;
    QString problem_string;
    QString job_string;
    int     nice_level;
};

class DVDPerfectThread : public JobThread
{
    //
    //  Fairly simple class that just knows
    //  how to copy
    //
    
  public:
  
    DVDPerfectThread(MTD *owner,
                     QMutex *drive_mutex, 
                     const QString &dvd_device, 
                     int track, 
                     const QString &dest_file, 
                     const QString &name,
                     const QString &start_string,
                     int nice_priority);

    ~DVDPerfectThread();
                     
    virtual void run();    

  private:
  
    RipFile      *ripfile;
    QMutex       *dvd_device_access;
    QString      dvd_device_location;
    int          dvd_title;
    dvd_reader_t *the_dvd;
    dvd_file_t   *title;
    unsigned char video_data[ 1024 * DVD_VIDEO_LB_LEN ];
};

#endif  // jobthread_h_


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
#include <qprocess.h>
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
    virtual bool transcodeSlotUsed(){return false;}
    
    QString getJobName();
    QString getSubName();
    double  getProgress(){return overall_progress;}
    double  getSubProgress(){return subjob_progress;}
    
    void    problem(const QString &a_problem);
    QString getProblem();
    QString getJobString();
    void    updateSubjobString( int seconds_elapsed, 
                                const QString &pre_string);
    void    cancelMe(bool yes_or_no){cancel_me = yes_or_no;}
    void    setSubProgress(double a_value, uint priority);
    void    setSubName(const QString &new_name, uint priority);
    virtual QString getFinalFileName(){return "";}
    void    sendLoggingEvent(const QString &event_string);
    
  protected:

    void setJobName(const QString &jname);
    void setProblem(const QString &prob);

  private:

    QString problem_string;
    QString job_name;
    QString subjob_name;
    QString job_string;

  protected:
  
    double  overall_progress;
    double  subjob_progress;
    double  sub_to_overall_multiple;
    MTD     *parent;
    int     nice_level;
    bool    cancel_me;
    
    QMutex  subjob_progress_mutex;
    QMutex  subjob_name_mutex;
    QMutex  problem_string_mutex;
    QMutex  job_name_mutex;
    QMutex  job_string_mutex;
};


class DVDThread : public JobThread
{

    //
    //  Base class for all *DVD* related
    //  job threads (perfect copy, transcode)
    //

  public:
  
    DVDThread(MTD *owner,
              QMutex *drive_mutex,
              const QString &dvd_device,
              int track,
              const QString &dest_file, 
              const QString &name,
              const QString &start_string,
              int nice_priority);

    ~DVDThread();
                     
    virtual void run();
    QString getFinalFileName(){return destination_file_string;}

  protected:

    bool         ripTitle(int title_number,
                          const QString &to_location,
                          const QString &extension,
                          bool multiple_files);
  
    RipFile      *ripfile;
    QMutex       *dvd_device_access;
    QString      dvd_device_location;
    QString      destination_file_string;
    int          dvd_title;
    dvd_reader_t *the_dvd;
    dvd_file_t   *title;
    unsigned char video_data[ 1024 * DVD_VIDEO_LB_LEN ];
    QString      rip_name;
};

class DVDISOCopyThread : public DVDThread
{
    //
    // Copy a byte-for-byte image of the disk
    // to an iso file.
    //

  public:

    DVDISOCopyThread(MTD *owner,
		     QMutex *drive_mutex, 
                     const QString &dvd_device, 
                     int track, 
                     const QString &dest_file, 
                     const QString &name,
                     const QString &start_string,
                     int nice_priority);

    ~DVDISOCopyThread();
                     
    virtual void run();

    bool copyFullDisc(void);
};

class DVDPerfectThread : public DVDThread
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
    

};

class DVDTranscodeThread : public DVDThread
{
    
    //
    //  An object that can rip a VOB off a DVD
    //  and then transcode it
    //
    
  public:
  
    DVDTranscodeThread(MTD *owner,
                       QMutex *drive_mutex,
                       const QString &dvd_device,
                       int track,
                       const QString &dest_file,
                       const QString &name,
                       const QString &start_string,
                       int nice_priority,
                       int quality_level,
                       bool do_ac3,
                       int which_audio,
                       int numb_seconds,
                       int subtitle_track_numb);
                       
                      
    ~DVDTranscodeThread();
    
    virtual void run();
    bool transcodeSlotUsed(){return used_transcode_slot;}
    
    bool    makeWorkingDirectory();
    bool    buildTranscodeCommandLine(int which_run);
    bool    runTranscode(int run);
    void    cleanUp();
    void    wipeClean();
    bool    used_transcode_slot;
    
  private:
  
    int          quality;
    QDir         *working_directory;
    QStringList  tc_arguments;
    QProcess     *tc_process;
    bool         two_pass;
    int          audio_track;
    int          length_in_seconds;
    bool         ac3_flag;
    int          subtitle_track;
};


#endif  // jobthread_h_


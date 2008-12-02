/*
    jobthread.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    Headers for the mtd threads that actually do things

*/

#ifndef JOBTHREAD_H_
#define JOBTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QStringList>

#include "fileobs.h"

class Q3Process;
class QDir;
class MythTranscodeDaemon;
class QWaitCondition;

/** \class JobThread
 *  \brief Base class for all MythTranscodeDaemon threads
 */
class JobThread : public QThread
{
  public:
    JobThread(MythTranscodeDaemon *owner,
              const QString       &start_string,
              int                  nice_priority);

    // Commands
    virtual void run(void);
    void    Cancel(bool chatty = false);

    // Gets
    QString GetJobName(void)     const;
    QString GetSubName(void)     const;
    QString GetLastProblem(void) const;
    QString GetJobString(void)   const;
    bool    IsCancelled(void)    const;
    double  GetProgress(void)    const { return overall_progress; }
    double  GetSubProgress(void) const { return subjob_progress;  }
    virtual QString GetFinalFileName(void) const { return ""; }
    virtual bool transcodeSlotUsed(void) const { return false; }
    virtual bool usesDevice(const QString &device) const
        { (void)device; return false; }

  protected:
    ~JobThread();

    // Commands
    void    SendProblemEvent(const QString &a_problem);
    void    SendLoggingEvent(const QString &event_string);
    void    UpdateSubjobString(int seconds_elapsed,
                               const QString &pre_string);
    // Sets
    void    SetSubProgress(double a_value, uint priority);
    void    SetSubName(const QString &new_name, uint priority);
    void    SetJobName(const QString &jname);
    void    SetLastProblem(const QString &prob);

  private:
    QString problem_string;
    QString job_name;
    QString subjob_name;
    QString job_string;

  protected:
    double  overall_progress;
    double  subjob_progress;
    double  sub_to_overall_multiple;
    MythTranscodeDaemon *parent;
    int     nice_level;

    mutable QMutex  subjob_progress_mutex;
    mutable QMutex  subjob_name_mutex;
    mutable QMutex  problem_string_mutex;
    mutable QMutex  job_name_mutex;
    mutable QMutex  job_string_mutex;

    QMutex         *cancelLock;
    QWaitCondition *cancelWaitCond;
    bool            cancel_me;
};
typedef QList<JobThread*> JobThreadList;


/** \class DVDThread
 *  \brief Base class for all *DVD* related job threads
 *         (perfect copy, transcode)
 */
class DVDThread : public JobThread
{
  public:

    DVDThread(MythTranscodeDaemon *owner,
              QMutex              *drive_mutex,
              const QString       &dvd_device,
              int                  track,
              const QString       &dest_file,
              const QString       &name,
              const QString       &start_string,
              int                  nice_priority);

    ~DVDThread();

    virtual void run(void);
    virtual QString GetFinalFileName(void) const
        { return destination_file_string; }
    bool    usesDevice(const QString &device) const
        { return dvd_device_location.contains(device); }

  protected:

    bool         ripTitle(int title_number,
                          const QString &to_location,
                          const QString &extension,
                          bool multiple_files,
                          QStringList *output_files = 0);

    QMutex      *dvd_device_access;
    QString      dvd_device_location;
    QString      destination_file_string;
    int          dvd_title;
    QString      rip_name;
};

    //
    // Copy a byte-for-byte image of the disk
    // to an iso file.
    //

class DVDISOCopyThread : public DVDThread
{
  public:
    DVDISOCopyThread(MythTranscodeDaemon *owner,
                     QMutex              *drive_mutex,
                     const QString       &dvd_device,
                     int                  track,
                     const QString       &dest_file,
                     const QString       &name,
                     const QString       &start_string,
                     int                  nice_priority);

    ~DVDISOCopyThread();

    virtual void run(void);

    bool copyFullDisc(void);
};

    //
    //  Fairly simple class that just knows
    //  how to copy
    //

class DVDPerfectThread : public DVDThread
{
  public:
    DVDPerfectThread(MythTranscodeDaemon *owner,
                     QMutex              *drive_mutex,
                     const QString       &dvd_device,
                     int                  track,
                     const QString       &dest_file,
                     const QString       &name,
                     const QString       &start_string,
                     int                  nice_priority);

    ~DVDPerfectThread();

    virtual void run(void);
};


    //
    //  An object that can rip a VOB off a DVD
    //  and then transcode it
    //

class DVDTranscodeThread : public DVDThread
{
  public:
    DVDTranscodeThread(MythTranscodeDaemon *owner,
                       QMutex              *drive_mutex,
                       const QString       &dvd_device,
                       int                  track,
                       const QString       &dest_file,
                       const QString       &name,
                       const QString       &start_string,
                       int                  nice_priority,
                       int                  quality_level,
                       bool                 do_ac3,
                       int                  which_audio,
                       int                  numb_seconds,
                       int                  subtitle_track_numb);
    ~DVDTranscodeThread();

    virtual void run(void);
    virtual bool transcodeSlotUsed(void) const { return used_transcode_slot; }

    bool    makeWorkingDirectory(void);
    bool    buildTranscodeCommandLine(int which_run);
    bool    runTranscode(int run);
    void    cleanUp(void);
    void    wipeClean(void);
    bool    used_transcode_slot;

  private:
    int          quality;
    QDir         *working_directory;
    QStringList  tc_arguments;
    Q3Process   *tc_process;
    bool         two_pass;
    int          audio_track;
    int          length_in_seconds;
    bool         ac3_flag;
    int          subtitle_track;
};


#endif  // jobthread_h_


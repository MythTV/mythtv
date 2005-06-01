#ifndef JOB_H_
#define JOB_H_
/*
	job.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for all kinds of transcoding jobs
*/

#include <iostream>
using namespace std;

#include <qthread.h>
#include <qstring.h>

class Transcoder;

class TranscoderJob: public QThread
{

  public:

    TranscoderJob(
                    Transcoder     *owner,
                    int             l_job_id
                 );
    virtual ~TranscoderJob();
    virtual void stop();

    void log(const QString &log_message, int verbosity);
    void warning(const QString &warn_message);
    int  getId();

  protected:
  
    bool            keep_going;
    QMutex          keep_going_mutex;
    Transcoder     *parent;
    int             job_id;

    QString         major_status_description;
    QString         minor_status_description;
    int             major_status;   //  0-100 inclusive
    int             minor_status;   //  0-100 inclusive
    QMutex          status_mutex;

};

#endif  // job_h_

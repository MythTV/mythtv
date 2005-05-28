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

class Transcoder;

class TranscoderJob: public QThread
{

  public:

    TranscoderJob(
                    Transcoder     *owner,
                    int             l_job_id
                 );
    virtual ~TranscoderJob();

  protected:
  
    Transcoder     *parent;
    int             job_id;

};

#endif  // job_h_

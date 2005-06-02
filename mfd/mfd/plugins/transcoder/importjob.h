#ifndef IMPORTJOB_H_
#define IMPORTJOB_H_
/*
	importjob.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for jobs that involve importing content
*/

#include <qdir.h>

#include "job.h"

class MetadataServer;

class ImportJob: public TranscoderJob
{

  public:

    ImportJob(
                    Transcoder     *owner,
                    int             l_job_id,
                    QString         l_scratch_dir_string,
                    QString         l_destination_dir_string,
                    MetadataServer *l_metadata_server,
                    int             l_container_id,
                    int             l_playlist_id
             );

    virtual ~ImportJob();
    virtual bool setupScratch();

  protected:
  
    QString         scratch_dir_string;
    QString         destination_dir_string;
    MetadataServer *metadata_server;
    int             container_id;
    int             playlist_id;
    QDir           *scratch_dir;
};

#endif  // importjob_h_

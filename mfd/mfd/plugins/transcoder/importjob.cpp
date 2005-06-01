/*
	importjob.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for jobs that involve importing content
*/

#include <qfileinfo.h>

#include "../../mdserver.h"
#include "importjob.h"


ImportJob::ImportJob(
                        Transcoder     *owner,
                        int             l_job_id,
                        QString         l_scratch_dir_string,
                        QString         l_destination_dir_string,
                        MetadataServer *l_metadata_server,
                        int             l_container_id
                    )
          :TranscoderJob(owner, l_job_id)
{
    scratch_dir_string = l_scratch_dir_string;
    destination_dir_string = l_destination_dir_string;
    metadata_server = l_metadata_server;
    container_id = l_container_id;
    scratch_dir = NULL;
}

bool ImportJob::setupScratch()
{
    //
    //  Create a working directory for this job's temporary files
    //
    
    QDir a_dir;
    
    
    
    if (! (a_dir.mkdir(QString("%1/job_%2").arg(scratch_dir_string).arg(job_id))))
    {
        warning("could not create scratch dir");
        return false;
    }

    scratch_dir = new QDir(QString("%1/job_%2").arg(scratch_dir_string).arg(job_id));
    QFileInfo scratch_checker(scratch_dir->path());
    if(!scratch_checker.isWritable())
    {
        warning(QString("scratch dir exists (\"%1\"), but "
                        "it is not writeable")
                        .arg(scratch_dir->path()));
        return false;
    }

    return true;
}

ImportJob::~ImportJob()
{
    if (scratch_dir)
    {
        //
        //  Try and delete it
        //

        if( !scratch_dir->rmdir(scratch_dir->path()))
        {
            warning(QString("could not remove scratch dir (%1), "
                            "you probably want to clean "
                            "it out by hand")
                            .arg(scratch_dir->path()));
        }
        
        
        delete scratch_dir;
        scratch_dir = NULL;
    }
}


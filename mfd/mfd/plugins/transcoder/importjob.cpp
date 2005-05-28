/*
	importjob.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for jobs that involve importing content
*/

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
}


ImportJob::~ImportJob()
{
}


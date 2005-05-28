/*
	job.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for all kinds of transcoding jobs
*/

#include <iostream>
using namespace std;

#include "transcoder.h"
#include "job.h"

TranscoderJob::TranscoderJob(Transcoder *owner, int l_job_id)
{
    parent = owner;
    job_id = l_job_id;
}

TranscoderJob::~TranscoderJob()
{
}

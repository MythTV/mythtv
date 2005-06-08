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
    keep_going = true;
}

void TranscoderJob::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

int  TranscoderJob::getId()
{
    int result = -1;

    status_mutex.lock();
        result = job_id;
    status_mutex.unlock();

    return result;
}

void TranscoderJob::log(const QString &log_message, int verbosity)
{
    QString log_string = QString("job %1: %2").arg(job_id).arg(log_message);
    if(parent)
    {
        parent->log(log_string, verbosity);
    }
    else
    {
        cout << "LOG: transcoder plugin: " << log_string << endl;
    }
}

void TranscoderJob::warning(const QString &warn_message)
{
    QString warn_string = QString("job %1: %2").arg(job_id).arg(warn_message);
    if(parent)
    {
        parent->warning(warn_string);
    }
    else
    {
        cout << "WARNING: transcoder plugin: " << warn_string << endl;
    }
}

QString TranscoderJob::getMajorStatusDescription()
{
    QString response;
    status_mutex.lock();
        response = major_status_description;
    status_mutex.unlock();
    return response;
}

int TranscoderJob::getMajorProgress()
{
    int response;
    status_mutex.lock();
        response = major_status;
    status_mutex.unlock();
    return response;
}

QString TranscoderJob::getMinorStatusDescription()
{
    QString response;
    status_mutex.lock();
        response = minor_status_description;
    status_mutex.unlock();
    return response;
}

int TranscoderJob::getMinorProgress()
{
    int response;
    status_mutex.lock();
        response = minor_status;
    status_mutex.unlock();
    return response;
}

void TranscoderJob::setMajorStatusDescription(const QString &new_status)
{
    status_mutex.lock();
        major_status_description = new_status;
    status_mutex.unlock();
}

void TranscoderJob::setMajorProgress(int new_progress)
{
    if(new_progress < 0)
    {
        new_progress = 0;
        warning("told to set major status below 0, changing to 0");
    }
    else if (new_progress > 100)
    {
        new_progress = 100;
        warning("told to set major status above 100, changing to 100");
    }
    status_mutex.lock();
        major_status = new_progress;
    status_mutex.unlock();
}

void TranscoderJob::setMinorStatusDescription(const QString &new_status)
{
    status_mutex.lock();
        minor_status_description = new_status;
    status_mutex.unlock();
}

void TranscoderJob::setMinorProgress(int new_progress)
{
    if(new_progress < 0)
    {
        new_progress = 0;
        warning("told to set minor status below 0, changing to 0");
    }
    else if (new_progress > 100)
    {
        new_progress = 100;
        warning("told to set minor status above 100, changing to 100");
    }
    status_mutex.lock();
        minor_status = new_progress;
    status_mutex.unlock();
}


TranscoderJob::~TranscoderJob()
{
}

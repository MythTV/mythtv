/*
	jobtracker.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    little object to keep track of jobs

*/

#include "jobtracker.h"

JobTracker::JobTracker(
                        int l_id, 
                        int l_major_progress, 
                        int l_minor_progress,
                        QString l_major_description,
                        QString l_minor_desription
                      )
{
    id = l_id;
    major_progress = l_major_progress;
    minor_progress = l_minor_progress;
    major_description = l_major_description;
    minor_description = l_minor_desription;
}

JobTracker::~JobTracker()
{
}

#ifndef JOBTRACKER_H_
#define JOBTRACKER_H_
/*
	jobtracker.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    little object to keep track of jobs

*/

#include <qstring.h>

class JobTracker
{

  public:

    JobTracker(
                int l_id, 
                int l_major_progress, 
                int l_minor_progress,
                QString l_major_description,
                QString l_minor_desription
              );

    ~JobTracker();

    int     getId(){ return id; }
    
    int     getMajorProgress(){ return major_progress; }
    int     getMinorProgress(){ return minor_progress; }
    
    QString getMajorDescription(){ return major_description; }
    QString getMinorDescription(){ return minor_description; }
 
  private:

    int     id;
    int     major_progress;
    int     minor_progress;
    QString major_description;
    QString minor_description;
};

#endif

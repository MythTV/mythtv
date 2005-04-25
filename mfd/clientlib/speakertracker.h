#ifndef SPEAKERTRACKER_H_
#define SPEAKERTRACKER_H_
/*
	speakertracker.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    little object to keep track of speakers

*/

#include <qstring.h>

class SpeakerTracker
{

  public:

    SpeakerTracker(int l_id, const QString yes_or_no);
    ~SpeakerTracker();
    
    void    markForDeletion(bool b){ marked_for_deletion = b; }
    bool    markedForDeletion(){ return marked_for_deletion; }
    bool    possiblyUnmarkForDeletion(const QString &id_string, const QString &inuse_string);
    bool    haveName();
    int     getId(){ return id; }
    QString getInUse(){ if (in_use) return QString("yes"); return QString("no"); }
    QString getName(){ return name; }
    void    setName(const QString &l_name){ name = l_name; }
 
  private:

    int     id;
    QString name;   
    bool    in_use;
    bool    marked_for_deletion;
};

#endif

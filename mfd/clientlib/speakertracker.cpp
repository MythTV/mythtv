/*
	speakertracker.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project

    little object to keep track of speakers

*/

#include <iostream>
using namespace std;

#include "speakertracker.h"


SpeakerTracker::SpeakerTracker(int l_id, const QString yes_or_no)
{
    id = l_id;
    if (yes_or_no == "yes")
    {
        in_use = true;
    }
    else if (yes_or_no == "no")
    {
        in_use = false;
    }
    else
    {
        cerr << "speaker tracker created with second argument neither "
             << "\"yes\" nor \"no\". Assuming no"
             <<endl;
    }
    marked_for_deletion = false;
    name = "";
}

bool SpeakerTracker::possiblyUnmarkForDeletion(
                                                const QString &id_string, 
                                                const QString &inuse_string
                                              )
{
    bool ok;
    int target_id = id_string.toInt(&ok);
    if (ok)
    {
        if (target_id == id)
        {
            marked_for_deletion = false;
            if (inuse_string == "yes")
            {
                in_use = true;
            }
            else if (inuse_string == "no")
            {
                in_use = false;
            } 
            else
            {
                cerr << "speaker tracker cannot determine if speaker is "
                     << "in use cause it was passed an in_use string of \""
                     << inuse_string
                     << "\""
                     << endl;
            }
            return true;
        }
    }
    else
    {
        cerr << "speaker tracker cannot check id cause it was "
             << "passed a string of \""
             << id_string
             << "\""
             << endl;
    }
    return false;
}

bool SpeakerTracker::haveName()
{
    if(name.length() < 1)
    {
        return false;
    }
    return true;
}

SpeakerTracker::~SpeakerTracker()
{
}


#ifndef DISCWATCHER_H_
#define DISCWATCHER_H_
/*
	discwatcher.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an optical disc watcher
*/

#include <iostream>
using namespace std; 

#include "mfd_plugin.h"

#include "devicewatcher.h"

class DiscWatcher: public MFDServicePlugin
{

  public:

    DiscWatcher(MFD *owner, int identity);
    ~DiscWatcher();
    
    void run();
    void buildDiscList();
    void checkDiscs();


  private:
  
    QPtrList<DeviceWatcher> device_watchers;

};

#endif  // discwatcher_h_

#ifndef MFDINSTANCE_H_
#define MFDINSTANCE_H_
/*
	mfdinstance.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	You get one of these objects for every mfd in existence

*/

#include <qobject.h>

class MfdInstance : public QObject
{

  public:

    MfdInstance();
    ~MfdInstance();
    
};

#endif

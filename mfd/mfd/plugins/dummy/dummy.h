#ifndef DUMMY_H_
#define DUMMY_H_
/*
	dummy.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a very basic and well documented
    mfd plugin.

*/

#include "../../mfdplugin.h"


class Dummy: public MFDCapabilityPlugin
{
    //
    //  This Dummy class inherits some basic
    //  capabilities from the MFDThreadPlugin class
    //  (see ../../mfdplugin.*)
    //

  public:

    Dummy(MFD *owner, int identity);
    ~Dummy();

    void    doSomething(const QStringList &tokens, int socket_identifier);

};

#endif  // dummy_h_
